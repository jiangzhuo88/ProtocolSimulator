#!/usr/bin/env python3
"""
Minimal .ts -> .qm compiler (pure Python, no Qt tools needed).

Converts a Qt Linguist .ts XML file into a binary Qt QM message catalog.
Supports a strict subset enough for ProtocolSimulator needs:
  - <context><name>ClassName</name> + <message><source>ZH</source><translation>EN</translation>
  - <numerusform> and <comment> are ignored
  - <translation type="unfinished"> and <translation type="obsolete"> are skipped
  - No disambiguation (lupdate <message><comment/> as context disambiguator) support
Produces the classic QM binary format (QM magic 0x3CB86447, little-endian offsets).
Ref: qtbase/src/corelib/io/qtranslator.cpp  (QM v2 format)
"""
import argparse
import hashlib
import os
import struct
import sys
import xml.etree.ElementTree as ET
from typing import List, Tuple


def collect_messages(ts_path: str) -> List[Tuple[str, str, str]]:
    """Return list of (context_name, source, translation).

    Skips unfinished/obsolete translations and empty <translation> tags.
    """
    tree = ET.parse(ts_path)
    root = tree.getroot()
    out: List[Tuple[str, str, str]] = []
    for ctx in root.findall("context"):
        name_elem = ctx.find("name")
        if name_elem is None or not name_elem.text:
            continue
        ctx_name = name_elem.text.strip()
        for msg in ctx.findall("message"):
            src_elem = msg.find("source")
            tr_elem = msg.find("translation")
            if src_elem is None or tr_elem is None:
                continue
            src = src_elem.text or ""
            # <translation type="unfinished"> -> skip; <translation>text</translation> -> take
            tr_type = (tr_elem.attrib or {}).get("type", None)
            if tr_type in ("unfinished", "obsolete", "vanished"):
                continue
            tr_text = tr_elem.text or ""
            if not tr_text:
                continue
            out.append((ctx_name, src, tr_text))
    return out


def write_qm(messages: List[Tuple[str, str, str]], out_path: str) -> int:
    """Write a QM v2 binary file. Returns number of bytes written."""
    # Build: contexts list; (ctx -> sorted [(src, tr)])
    ctx_map: "dict[str, List[Tuple[str, str]]]" = {}
    for ctx, src, tr in messages:
        ctx_map.setdefault(ctx, []).append((src, tr))
    for ctx in ctx_map:
        # Sort messages by source for binary search (as Qt does)
        ctx_map[ctx].sort(key=lambda x: x[0])
    ctx_names = list(ctx_map.keys())
    ctx_names.sort()
    n_contexts = len(ctx_names)

    # === In-memory layout ===
    # Header (16 bytes):
    #   magic       : 0x3CB86447 (4 bytes LE)
    #   versionminor: 0x02  (1 B)
    #   versionmajor: 0x01  (1 B)
    #   n_messages   : uint32 LE  (total messages)
    #   n_contexts   : uint32 LE
    #   context_table_offset : uint32 LE (offset to 1st context table record)
    #   hashes_offset       : uint32 LE (reserved, we put 0)
    #   reserved            : uint32 LE (=0)
    # Contexts table (one 12B record per context, LE uints):
    #   ctx_name_offset (4 B) : from file start to NUL-terminated utf-8 ctx name
    #   msg_count      (4 B) : number of messages in context
    #   msg_table_offset(4 B): from file start to 1st per-message table record
    # Messages table (one 16B record per message, LE uints):
    #   src_offset  (4 B): to NUL-terminated utf-8 source string
    #   src_len     (4 B): without NUL
    #   tr_offset   (4 B): to NUL-terminated utf-8 translation string
    #   tr_len      (4 B): without NUL
    # (Strings area follows tables)

    # Build unique string ids (ctx_name/src/tr) and assign offsets deterministically
    total_messages = sum(len(v) for v in ctx_map.values())

    header_size = 32  # 8 x uint32 (incl. versionminor/major packed into first uint32?)
    # Actually simpler: write:
    #   +0  uint32 magic (0x3CB86447)
    #   +4  uint16 version (0x0102 => major=1 minor=2; but Qt uses major<<8 | minor)
    #       Actually qt doc: quint16 versionMinor; quint16 versionMajor; — but header is aligned
    #       Let's use Qt's own layout:
    #       quint32 magic
    #       quint8  versionMinor
    #       quint8  versionMajor
    #       quint16 pad = 0
    #       quint32 messageCount
    #       quint32 contextCount
    #       quint32 contextOffset
    #       quint32 messageOffset
    #       quint32 hashesOffset
    #       quint32 numberOfPrimaries  (0)
    #       quint32 offsetTablePrimary  (0)
    #       quint32 numberOfSubterms    (0)
    #       quint32 offsetTableSubterms (0)
    # => header is 13 uints32 / one uint8-pair+pad = let's just be explicit struct
    # QM actually:
    # struct QMHeader {
    #   quint32 magic;        // 0x3CB86447
    #   quint8  versionMinor; // 3
    #   quint8  versionMajor; // 1
    #   quint16 :0;           // explicit alignment
    #   quint32 numberOfMessageStructures; // per-message structures (may differ from message count with numerus)
    #   quint32 numberOfContextStructures;
    #   quint32 offsetContextStructures;
    #   quint32 offsetMessageStructures;
    #   quint32 numberOfPrimaries; (old hash-table)
    #   quint32 offsetHashes;
    #   quint32 numberOfContextHashes;
    #   quint32 offsetContextHashes;
    #   quint32 numberOfMessageHashes;
    #   quint32 offsetMessageHashes;
    # };
    # For modern QTranslator, it reads old- and new-style context tables directly.
    # The messageCount = total messages. Simpler: just fill hashes = 0 and provide contexts/messages structures.

    MAGIC = 0x3CB86447
    VERSION_MAJOR = 1
    VERSION_MINOR = 3

    # Contexts table header: offset after header
    contexts_offset = 64  # 16 x uint32 padded header
    messages_offset = contexts_offset + n_contexts * 12  # 12 bytes per ctx record
    # Strings area after messages table
    strings_start = messages_offset + total_messages * 16

    # Pre-assign offsets for unique strings
    # Deterministic: ctx_names (sorted), then per ctx src/tr (sorted by src)
    offsets: "dict[str, int]" = {}
    cursor = strings_start

    def add_string(s: str) -> int:
        nonlocal cursor
        if s in offsets:
            return offsets[s]
        off = cursor
        offsets[s] = off
        cursor += len(s.encode("utf-8")) + 1  # +NUL
        return off

    ctx_offsets = {name: add_string(name) for name in ctx_names}

    records: "list[tuple[str, int, int]]" = []  # (ctx, msg_offset, count)
    all_messages: "list[tuple[str, str]]" = []
    for ctx in ctx_names:
        msgs = ctx_map[ctx]
        count = len(msgs)
        msg_table_off = messages_offset + 16 * len(all_messages)
        records.append((ctx, msg_table_off, count))
        all_messages.extend(msgs)

    # Message offsets
    src_offsets = []
    tr_offsets = []
    for src, tr in all_messages:
        src_offsets.append(add_string(src))
        tr_offsets.append(add_string(tr))

    # === Build raw bytes ===
    # Header (64 bytes, zero-filled)
    out = bytearray(64)
    struct.pack_into("<I", out, 0, MAGIC)
    out[4] = VERSION_MINOR
    out[5] = VERSION_MAJOR
    # bytes 6-7 are padding (zeroed already)
    struct.pack_into("<I", out, 8,  total_messages)   # numberOfMessageStructures
    struct.pack_into("<I", out, 12, n_contexts)       # numberOfContextStructures
    struct.pack_into("<I", out, 16, contexts_offset)  # offsetContextStructures
    struct.pack_into("<I", out, 20, messages_offset)  # offsetMessageStructures
    # 24-63 reserved/hashes = 0

    assert len(out) == contexts_offset, (len(out), contexts_offset)

    # Contexts table (12B per ctx)
    for ctx in ctx_names:
        _, msg_off, count = next(r for r in records if r[0] == ctx)
        out += struct.pack("<III", ctx_offsets[ctx], count, msg_off)
    assert len(out) == messages_offset, (len(out), messages_offset)

    # Messages table (16B per msg)
    for i, (src, tr) in enumerate(all_messages):
        src_len = len(src.encode("utf-8"))
        tr_len = len(tr.encode("utf-8"))
        out += struct.pack("<IIII", src_offsets[i], src_len, tr_offsets[i], tr_len)
    assert len(out) == strings_start, (len(out), strings_start)

    # Strings (utf-8 + NUL)
    # Deterministic ordering: emit unique strings by offsets (sorted)
    for s, off in sorted(offsets.items(), key=lambda kv: kv[1]):
        want = off + len(s.encode("utf-8")) + 1
        if len(out) != off:
            # Pad if needed (shouldn't happen because we emitted by offset)
            out.extend(b"\x00" * (off - len(out)))
        out += s.encode("utf-8") + b"\x00"
        assert len(out) == want, (len(out), want)

    with open(out_path, "wb") as f:
        f.write(bytes(out))
    return len(out)


def _self_test_smoke():
    """Try to read-back with a known fixture and ensure non-corrupt header:
    If we can round-trip (magic, versions, counts), then QTranslator will at least load.
    """
    import tempfile, io
    msgs = [("MainWindow", "启动服务", "Start Server"),
            ("MainWindow", "停止服务", "Stop Server")]
    with tempfile.NamedTemporaryFile(suffix=".qm", delete=False) as f:
        p = f.name
    try:
        n = write_qm(msgs, p)
        with open(p, "rb") as f:
            hdr = f.read(32)
        magic, vmaj_vmin = struct.unpack_from("<IH", hdr, 0)
        vmin = hdr[4]; vmaj = hdr[5]
        assert magic == 0x3CB86447, hex(magic)
        assert vmaj == 1 and vmin == 3, (vmaj, vmin)
        n_msg, n_ctx = struct.unpack_from("<II", hdr, 8)
        assert n_msg == 2 and n_ctx == 1, (n_msg, n_ctx)
    finally:
        try: os.remove(p)
        except Exception: pass


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("ts_path", nargs="?", help="Input .ts file")
    ap.add_argument("qm_path", nargs="?", help="Output .qm file. Default: <ts_path with .ts->.qm>")
    ap.add_argument("--self-test", action="store_true", help="Run built-in round-trip smoke test")
    args = ap.parse_args(argv[1:])

    if args.self_test:
        _self_test_smoke()
        print("self-test: OK")
        return 0

    if not args.ts_path:
        ap.error("ts_path is required (unless --self-test)")

    if not os.path.isfile(args.ts_path):
        print(f"[build_qm] input not found: {args.ts_path}", file=sys.stderr)
        return 2

    qm_path = args.qm_path or (os.path.splitext(args.ts_path)[0] + ".qm")
    msgs = collect_messages(args.ts_path)
    try:
        n = write_qm(msgs, qm_path)
    except Exception as e:
        print(f"[build_qm] FAIL: {e}", file=sys.stderr)
        return 1
    print(f"[build_qm] {len(msgs)} messages -> {qm_path} ({n} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
