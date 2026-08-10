#ifndef IHR_TABLEVIEW_H
#define IHR_TABLEVIEW_H

#include <assert.h>
#include "string.h"
#include <vector>
#include <iostream>
using namespace std;

#ifdef _WIN32
#include <Windows.h>
#ifdef HRTABLEVIEW_LIB
#define HRTABLEVIEW_EXPORT __declspec(dllexport)
#else
#define HRTABLEVIEW_EXPORT __declspec(dllimport)
#endif
#else
#include <stdint.h>
typedef int64_t    INT64;		//add by yhh 20140710
#ifdef HRTABLEVIEW_LIB
#define HRTABLEVIEW_EXPORT
#else
#define HRTABLEVIEW_EXPORT
#endif
#endif

//笔刷样式
enum PenStyle
{
	NoPen	= 0,
	SolidLine,	
	DashLine,	
	DotLine,	
	DashDotLine,	
	DashDotDotLine,	
	CustomDashLine
};

//排序方式
enum SortOrder
{
	AscendingOrder,
    DescendingOrder
};

//选择方式
enum SelectionBehavior
{
	SelectItems,
	SelectRows,
	SelectColumns
};

//选择模式
enum SelectionMode
{
	NoSelection,	
	SingleSelection = 1,    //单选
	ContiguousSelection = 4, //可复选
	ExtendedSelection = 3,	//结合ctrl键可复选
	
};

//选择状态
enum ItemCheckState
{
	ItemUnchecked,
	ItemPartiallyChecked,
	ItemChecked
};

enum ResizeMode
{
	Interactive,
	Fixed,
	Stretch,
	ResizeToContents
};

//对齐方式
enum AlignmentFlag {
	AlignLeft = 0x0001,
	AlignLeading = AlignLeft,
	AlignRight = 0x0002,
	AlignTrailing = AlignRight,
	AlignHCenter = 0x0004,
	AlignJustify = 0x0008,
	AlignAbsolute = 0x0010,
	AlignHorizontal_Mask = AlignLeft | AlignRight | AlignHCenter | AlignJustify | AlignAbsolute,

	AlignTop = 0x0020,
	AlignBottom = 0x0040,
	AlignVCenter = 0x0080,
	AlignVertical_Mask = AlignTop | AlignBottom | AlignVCenter,
	AlignCenter = AlignVCenter | AlignHCenter
};

//回滚方式 add 20160128
enum ScrollFlag {
	ScrollToTop = 0,		// 回滚到顶部	
	ScrollToCenter,			// 回滚到中部
	ScrollToVisible			// 回滚到可见范围内
};

//begin
enum FocusReason{
	MouseFocusReason,
	TabFocusReason,
	BacktabFocusReason,
	ActiveWindowFocusReason,
	PopupFocusReason,
	ShortcutFocusReason,
	MenuBarFocusReason,
	OtherFocusReason,
	NoFocusReason
};
//end

class ITableObserver 
{
public:
	ITableObserver(){}
	virtual ~ITableObserver(){}
	//row和column均为逻辑行号和列号
public:
	virtual void	leftClicked(int logicalRow, int logicalColumn){}
	virtual void    rightClicked(int logicalRow, int logicalColumn){}
	virtual void    midClicked(int logicalRow, int logicalColumn){}
	virtual void	leftDoubleClicked(int logicalRow, int logicalColumn){}
	virtual void	rightDoubleClicked(int logicalRow, int logicalColumn){}
	virtual void	entered(int logicalRow, int logicalColumn){}
	virtual void	pressed(int logicalRow, int logicalColumn){}
	virtual void	mouseMove(int logicalRow, int logicalColumn){}		//add by jx 20160824
	virtual void	dataChanged(int logicalRow, int logicalColumn){}    //add by yuanx 20170109

	//add by jx-20140923 增添键盘键操作消息
	virtual void	keyPressed(int key, int modifier, int logicalRow){}
	virtual void	contextMenuEvent(int posX, int posY){}

	//modified by jx 20141104 ---返回逻辑索引号
	virtual void	headerRightClicked(int nLogicalIndex){}
	virtual void	headerLeftClicked(int nLogicalIndex){}
	virtual void	headerMidClicked(int nLogicalIndex){}

	//add by hef 20170104 增添鼠标释放操作消息
	virtual void	leftRelease(int logicalRow, int logicalColumn){}
	//add by hef 20170106 增添表格选中行改变操作消息
	virtual void	selectChanged(){}
	
	//add by hef 20171225 增添表格表头顺序改变操作消息
	virtual void	headerSectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex){}
	
	//add by yuanx 20170523 滚动条变化通知消息, nValue为滚动条所在的表格行号
	virtual void	verticalScrollBar_ValueChanged(int nValue) {}
	
	//add by wujp 20170517 QComboBox下拉选项发生改变通知消息
	virtual void comboxCurrentIndexChanged(int logicalRow, int logicalColumn, const char *pText){}
};

class IDynamicFillterSink
{
public:
	virtual ~IDynamicFillterSink(){}

	virtual bool	filterAcceptsRow(int row){return true;}
	virtual bool	lessThan(int leftRow, int leftcolumn, 
		int rightRow, int rightColumn) const
	{
		return true;
	}
};

//  [5/22/2017 jx] 修改HrString，限定长度最大256个字节
const int HRSTRING_MAXLENGTH = 256;
class HrString
{
public:
	HrString()
	{
		m_DataLen = 0;
		memset(m_Data, 0, sizeof(m_Data));
	}
	HrString(const char* pStr)
	{
		//assert(strlen(pStr) < 128);
		m_DataLen = 0;
		memset(m_Data, 0, sizeof(m_Data));
		m_DataLen = strlen(pStr);
		m_DataLen = (m_DataLen > HRSTRING_MAXLENGTH) ? HRSTRING_MAXLENGTH : m_DataLen;	//  [5/22/2017 jx] 限定长度
		memcpy(m_Data, pStr, m_DataLen);	
	}

	HrString(const HrString& str)
	{
		memset(m_Data, 0, sizeof(m_Data));
		m_DataLen = strlen(str.m_Data);
		m_DataLen = (m_DataLen > HRSTRING_MAXLENGTH) ? HRSTRING_MAXLENGTH : m_DataLen;	//  [5/22/2017 jx] 限定长度
		memcpy(m_Data, str.m_Data, m_DataLen);
	}
	virtual ~HrString(){}

	HrString& operator = (const HrString& str)
	{
		memset(m_Data, 0, sizeof(m_Data));
		m_DataLen = strlen(str.m_Data);
		m_DataLen = (m_DataLen > HRSTRING_MAXLENGTH) ? HRSTRING_MAXLENGTH : m_DataLen;	//  [5/22/2017 jx] 限定长度
		memcpy(m_Data, str.m_Data, m_DataLen);
		return *this;
	}

	HrString& operator = (const char* pStr)
	{
		memset(m_Data, 0, sizeof(m_Data));
		m_DataLen = strlen(pStr);
		m_DataLen = (m_DataLen > HRSTRING_MAXLENGTH) ? HRSTRING_MAXLENGTH : m_DataLen;	//  [5/22/2017 jx] 限定长度
		memcpy(m_Data, pStr, m_DataLen);
		return *this;
	}

	const char* c_str()
	{
		return m_Data;
	}

	int length() const
	{
		return m_DataLen;
	}
	//add by hef 20170927
	bool isEmpty()
	{
		return (m_DataLen == 0);
	}

protected:
	char m_Data[HRSTRING_MAXLENGTH];		//modified 20160303 暂时修改为m_Data[256], 3X8字符串超过128
	int  m_DataLen;
};

typedef vector<HrString> StringList;

struct CellIndex 
{
	int row;
	int column;
	CellIndex()
	{
		row = -1;
		column = -1;
	}
};

typedef vector<CellIndex> CellIndexList;

//----------------------------------------------------
class HRTABLEVIEW_EXPORT IIcon
{
public:
	IIcon(){}
	virtual ~IIcon(){}
public:
	static  IIcon* createIcon(const char* filename);
	virtual bool	isNull () const = 0;
	virtual const char*	name () const = 0;
	virtual void*   getInerIcon() = 0;
};
//----------------------------------------------------
class QPalette;
class QValidator;
class HRTABLEVIEW_EXPORT IHrTableView
{
public:
	IHrTableView(){}
	virtual ~IHrTableView(){}

public:
	/*************************************************************************
	*函数名：createTableView
	*功能：创建表格视图
	*参数：parentWnd：父句柄
			FontName：字体类型，默认为NULL，表示采用系统默认字体
			FontSize：字体大小
			isBold：粗体标记
	*返回值：IHrTableView*：成功，null：失败
	*************************************************************************/
    static IHrTableView* createTableView(void* parentWnd, const char* FontName = NULL, const int FontSize = 9, bool isBold = false);
	
	//add 20151023
	/*************************************************************************
	*函数名：deleteTableView
	*功能：释放表格视图资源
	*参数：pTableView：表格视图指针
	*返回值：无
	*************************************************************************/
	static void deleteTableView(IHrTableView* pTableView);

	/*************************************************************************
	*函数名：setSpan
	*功能：设置间隔数
	*参数：row：行号 column：列号 rowSpanCount：行间隔  columnSpanCount：列间隔（可视行号、列号）
	*返回值：void
	*************************************************************************/
	virtual	void	setSpan ( int row, int column, int rowSpanCount, int columnSpanCount ) = 0;

	/************************************************************************/
	/*
	名称：getVersion()
	注释：获取版本信息
	返回：string
	*/
	/************************************************************************/
	virtual string getVersion() = 0;

	/*************************************************************************
	*函数名：getWindowHandle
	*功能：获取窗口句柄
	*参数：
	*返回值：HWND：返回句柄
	*************************************************************************/
	virtual long    getWindowHandle() = 0;

	/*************************************************************************
	*函数名：columnAt
	*功能：获取列号（可视列号）
	*参数：x：鼠标位置的X坐标值
	*返回值：< -1 :x值无效,>= 0:列号，第几列
	*************************************************************************/
	virtual int			columnAt(int x) const = 0;

	/*************************************************************************
	*函数名：columnWidth
	*功能：获取column列的宽度
	*参数：column：列号（可视列号）
	*返回值：<= 0 :有错,> 0:column列的宽度
	*************************************************************************/
	virtual int			columnWidth(int column) const = 0;

	/*************************************************************************
	*函数名：gridStyle
	*功能：获取插入的格子样式
	*参数：无
	*返回值：PenStyle：画笔的样式
	*************************************************************************/
	virtual PenStyle	gridStyle() const = 0;

	/*************************************************************************
	*函数名：isColumnHidden
	*功能：查看是否将column列进行隐藏/不可见
	*参数：column：第几列（可视列号）
	*返回值：true：成功，false:失败
	*************************************************************************/
	virtual bool		isColumnHidden(int column) const = 0;

	/*************************************************************************
	*函数名：isRowHidden
	*功能：是否将行进行隐藏
	*参数：row：第几行（可视行号）
	*返回值：true：成功，false:失败
	*************************************************************************/
	virtual bool		isRowHidden(int row) const = 0;

	/*************************************************************************
	*函数名：isSortingEnabled
	*功能：获取表排序使能标记
	*参数：无
	*返回值：true：表已设置排序，false:表未设置排序
	*************************************************************************/
	virtual bool		isSortingEnabled() const = 0;

	/*************************************************************************
	*函数名：rowAt
	*功能：获取行的位置
	*参数：y:鼠标位置的Y坐标值
	*返回值：< -1 :x值无效,>= 0:行号，第几行
	*************************************************************************/
	virtual int			rowAt(int y) const = 0;

	/*************************************************************************
	*函数名：rowHeight
	*功能：返回给定行row的高度
	*参数：row:第几行（可视行号）
	*返回值：<= 0 :row有错,> 0:记录行的高度
	*************************************************************************/
	virtual int			rowHeight(int row) const = 0;

	/*************************************************************************
	*函数名：setColumnHidden
	*功能：将列设置为隐藏/不可见
	*参数：column:在第几列,hide:隐藏（可视列号）
	*返回值：void
	*************************************************************************/
	virtual void		setColumnHidden(int column, bool hide) = 0;

	/*************************************************************************
	*函数名：setColumnWidth
	*功能：设置列的宽度
	*参数：column:列的位置,width:宽度（可视列号）
	*返回值：void
	*************************************************************************/
	virtual void		setColumnWidth(int column, int width) = 0;	

	/*************************************************************************
	*函数名：setCornerButtonEnabled
	*功能：设置左上角按钮的使能标记
	*参数：enable：可操作的，有效地
	*返回值：void
	*************************************************************************/
	virtual void		setCornerButtonEnabled( bool enable) = 0;

	/*************************************************************************
	*函数名：setAlternatingRowColors
	*功能：设置是否允许表格行背景交叉显示
	*参数：enable：有效地
	*返回值：
	*************************************************************************/
	//virtual void		setAlternatingRowColors(bool enable, COLORREF rgbVal) = 0;
	
	/*************************************************************************
	*函数名：setGridStyle
	*功能：设置格子样式
	*参数：style：样式
	*返回值：void
	*************************************************************************/
	virtual void		setGridStyle(PenStyle style) = 0;

	/*************************************************************************
	*函数名：setRowHeight
	*功能：设置行的高度
	*参数：row:行的位置,height:行的高度（可视行号）
	*返回值：void
	*************************************************************************/
	virtual void		setRowHeight(int row, int height) = 0;

	/*************************************************************************
	*函数名：setRowHidden
	*功能：将行设置为隐藏/不可见
	*参数：row:行的位置 ,hide：隐藏/不可见（可视行号）
	*返回值：void
	*************************************************************************/
	virtual void		setRowHidden(int row, bool hide) = 0;

	/*************************************************************************
	*函数名：setSortingEnabled
	*功能：设置排序使能
	*参数：enable：排序使能，true:排序，false:不排序
	*返回值：void
	*************************************************************************/
	virtual void		setSortingEnabled(bool enable) = 0;

	/*************************************************************************
	*函数名：showGrid
	*功能：显示表格的格子
	*参数：void
	*返回值：true：成功，false:失败
	*************************************************************************/
	virtual bool		showGrid() const = 0;

	/*************************************************************************
	*函数名：sortByColumn
	*功能：设置列的排序顺序
	*参数：column:列号, order:排序顺序，升序或降序（可视列号）
	*返回值：void
	*************************************************************************/
	virtual void		sortByColumn(int column, SortOrder order) = 0;

	/*************************************************************************
	*函数名：hideColumn
	*功能：将列设置为隐藏/不可见
	*参数：column:列的位置（可视列号）
	*返回值：void
	*************************************************************************/
	virtual void		hideColumn(int column) = 0;

	/*************************************************************************
	*函数名：hideRow
	*功能：将行设置为隐藏/不可见,
	*参数：row:行的位置（可视行号）
	*返回值：void
	*************************************************************************/
	virtual void		hideRow(int row) = 0;

	/*************************************************************************
	*函数名：resizeColumnToContents
	*功能：根据内容自动调整指定列的列宽
	*参数：column:列的位置（可视列号）
	*返回值：void
	*************************************************************************/
	virtual void		resizeColumnToContents(int column) = 0;

	/*************************************************************************
	*函数名：resizeColumnsToContents
	*功能：根据内容自动调整表格的列宽
	*参数：void
	*返回值：void
	*************************************************************************/
	virtual void		resizeColumnsToContents() = 0;

	/*************************************************************************
	*函数名：resizeRowToContents
	*功能：根据内容自动调整指定行的行高
	*参数：row:行的位置（可视行号）
	*返回值：void
	*************************************************************************/
	virtual void		resizeRowToContents(int row) = 0;

	/*************************************************************************
	*函数名：resizeRowsToContents
	*功能：根据内容自动调整表格的行高
	*参数：void
	*返回值：void
	*************************************************************************/
	virtual void		resizeRowsToContents () = 0;

	/*************************************************************************
	*函数名：selectColumn
	*功能：选择列
	*参数：column:列的位置（可视列号）
	*返回值：void
	*************************************************************************/
	virtual void		selectColumn(int column ) = 0;

	/*************************************************************************
	*函数名：selectRow
	*功能：选择行
	*参数：row:行的位置（可视行号）
	*返回值：void
	*************************************************************************/
	virtual void		selectRow(int row ) = 0;

	/*//add by hef 20170105************************************************************************
	*函数名：selectRows
	*功能：选择多行
	*参数：row:行的位置（可视行号）,cnt:行数
	*返回值：void
	*************************************************************************/
	virtual void		selectRows(int *row, int cnt) = 0;

	/*************************************************************************
	*函数名：setShowGrid
	*功能：设置显示表格的格子
	*参数：show：显示
	*返回值：void
	*************************************************************************/
	virtual void		setShowGrid(bool show ) = 0;

	/*************************************************************************
	*函数名：showColumn
	*功能：显示指定列
	*参数：column：列的位置（可视列号）
	*返回值：void
	*************************************************************************/
	virtual void		showColumn(int column ) = 0;

	/*************************************************************************
	*函数名：showRow
	*功能：显示指定行
	*参数：row:行的位置（可视行号）
	*返回值：void
	*************************************************************************/
	virtual void		showRow(int row ) = 0;

	/*************************************************************************
	*函数名：clearSelection
	*功能：清除所有选中的单元格为未选中状态
	*参数：void
	*返回值：void
	*************************************************************************/
	virtual void		clearSelection() = 0;

	/*************************************************************************
	*函数名：scrollToRow  --add 20160128
	*功能：将某行某列回滚到可见范围内某位置，列号默认为0，位置默认顶部
	*参数：row 行号(逻辑行,从0开始), column 列号(逻辑列), position 0:顶部 1:中部 其他值:可见
	*返回值：void
	*************************************************************************/
	virtual	void		scrollToRow(int row, int column = 0, int position = ScrollToTop) = 0;

	/*************************************************************************
	*函数名：scrollToBottom
	*功能：回滚到底部
	*参数：void
	*返回值：void
	*************************************************************************/
	virtual	void		scrollToBottom() = 0;

	/*************************************************************************
	*函数名：scrollToTop
	*功能：回滚到顶部
	*参数：void
	*返回值：void
	*************************************************************************/
	virtual	void		scrollToTop() = 0;

	/*************************************************************************
	*函数名：selectAll
	*功能：全选
	*参数：void
	*返回值：void
	*************************************************************************/
	virtual	void		selectAll() = 0;

	/*************************************************************************
	*函数名：setCurrentItem
	*功能：设置指定位置(row, column)的单元格为当前Item
	*参数：row:行的位置 ,column:列的位置
	*返回值：void
	*************************************************************************/
	//virtual	void		setCurrentItem(int row, int column) = 0;

	/*************************************************************************
	*函数名：getCurrentCellIndex
	*功能：获取当前Item的单元格位置，（逻辑行号，列号）
	*参数：void
	*返回值：CellIndex：单元格位置
	*************************************************************************/
	virtual CellIndex   getCurrentCellIndex() = 0;

	/*************************************************************************
	*函数名：setToolTipVisible(add by hef 2018.1.9)
	*功能：设置表格是否显示提示信息
	*参数：bVisible:true显示 false不显示
	*返回值：void
	*************************************************************************/
	virtual void   setToolTipVisible(bool bVisible) = 0;

	/*************************************************************************
	*函数名：getSelectedIndexes
	*功能：获取选中Item的单元格列表，（逻辑行号，列号）
	*参数：void
	*返回值：CellIndexList：单元格位置列表
	*************************************************************************/
	virtual CellIndexList        getSelectedIndexes() = 0;

	/*************************************************************************
	*函数名：setTableBackgroundColor
	*功能：设置表格背景颜色
	*参数：颜色值color
	*返回值：void
	*************************************************************************/
	virtual void		setTableBackgroundColor(const char* color) = 0;

	/*************************************************************************
	*函数名：setSelectionBehavior
	*功能:设置选中行为
	*参数：behavior:选中单元格，选中行，选中列
	*返回值：void
	*************************************************************************/
	virtual void		setSelectionBehavior (SelectionBehavior behavior) = 0;

	/*************************************************************************
	*函数名：setSelectionMode
	*功能：设置选中模式
	*参数：mode:模式
	*返回值：void
	*************************************************************************/
	virtual	void		setSelectionMode (SelectionMode mode) = 0;

	/*************************************************************************
	*函数名：selectionBehavior
	*功能：获取当前选中行为属性
	*参数：void
	*返回值：SelectionBehavior：选中行为属性，详见枚举变量定义
	*************************************************************************/
	virtual	SelectionBehavior	selectionBehavior () const = 0;

	/*************************************************************************
	*函数名：selectionMode
	*功能：返回当前的选中模式属性
	*参数：void
	*返回值：SelectionMode：选中模式属性，选项见枚举变量定义
	*************************************************************************/
	virtual	SelectionMode	selectionMode() const = 0;

	/*************************************************************************
	*函数名：setHeaderMovable
	*功能：设置表头移动标记
	*参数：bMovable：是/否可移动
	*返回值：void
	*************************************************************************/
	virtual void        setHeaderMovable(bool bMovable) = 0;

	/*************************************************************************
	*函数名：isHeaderMovable
	*功能：获取表头移动标记
	*参数：void
	*返回值：true:成功,false:失败
	*************************************************************************/
	virtual bool		isHeaderMovable() const = 0;

	/*************************************************************************
	*函数名：setVerticalHearderShow
	*功能：设置垂直表头显示使能
	*参数：bShow:是/否显示
	*返回值：void
	*************************************************************************/
	virtual void        setVerticalHeaderShow(bool bShow) const = 0;

	/*************************************************************************
	*函数名：sethorizontalHeaderShow
	*功能：设置水平表头显示使能
	*参数：bShow:是/否显示
	*返回值：void
	*************************************************************************/
	virtual void        sethorizontalHeaderShow(bool bShow) const = 0;

	/*************************************************************************
	*函数名：setHeaderResizeMode
	*功能：设置表头调整内容大小的模式
	*参数：mode:调整内容大小
	*返回值：void
	*************************************************************************/
	virtual void		setHeaderResizeMode(ResizeMode mode) = 0;

	/*************************************************************************
	*函数名：getHeaderResizeMode
	*功能：获取表头调整内容大小的模式
	*参数：void
	*返回值：ResizeMode：调整内容大小
	*************************************************************************/
	virtual ResizeMode  getHeaderResizeMode() = 0;
	
	/*add by jx 20160922***********增加接口************************************
	*函数名：setHeaderStretchLastSection
	*功能：设置表头最后一列是否按表格宽度自动调整
	*参数：bStretch:true最后一列按表格宽度自动调整 false不调整
	*返回值：void
	*************************************************************************/
	virtual void		setHeaderStretchLastSection(bool bStretch) = 0;

	/*************************************************************************
	*函数名：setStyleSheet
	*功能：设置样式表
	*参数：strStyle：样式
	*返回值：
	*************************************************************************/
	virtual void        setStyleSheet(const char* strStyle) = 0;

	/*************************************************************************
	*函数名：setStyleSheet
	*功能：设置表头的样式 [8/19/2013   Zhangjy]
	*参数：strStyle：样式
	*返回值：void
	*************************************************************************/
	virtual void		setHeaderViewStyleSheet(const char* strStyle) = 0;
	
	/*************************************************************************
	*函数名：setTableTextAlignment
	*功能：设置表格文字的对齐方式 
	*参数：alignment：对齐方式
	*返回值：void
	*************************************************************************/
	virtual void        setTableTextAlignment(AlignmentFlag alignment) = 0;
	
	/*************************************************************************
	*函数名：setFocus
	*功能：设置获得焦点的原因 
	*参数：FocusReason：获得焦点原因
	*返回值：void
	*************************************************************************/
	virtual void		setFocus(FocusReason reason) = 0;
	
	//add by jx 20141031--增添接口
	/*************************************************************************
	*函数名：getVisualRow
	*功能：根据逻辑行号获得对应的可视行号
	*参数：logicalRow:逻辑行号
	*返回值：可视行号
	*************************************************************************/
	virtual int			getVisualRow(int logicalRow) = 0;

	/*************************************************************************
	*函数名：getLogicalRow
	*功能：根据可视行号获得对应的逻辑行号
	*参数：visualRow：可视行号
	*返回值：逻辑行号
	*************************************************************************/
	virtual int			getLogicalRow(int visualRow) = 0;


public:
	
	/*************************************************************************
	*函数名：setMaxColumnCount
	*功能：设置最大列数
	*参数：column:列数目
	*返回值：void
	*************************************************************************/
	virtual void	setMaxColumnCount(int columns) = 0;

	/*************************************************************************
	*函数名：insertColumn
	*功能：在指定column列处插入1列
	*参数：column:列的位置（可视列号）
	*返回值：void
	*************************************************************************/
	virtual void	insertColumn(int column) = 0;

	/*************************************************************************
	*函数名：insertRow
	*功能：在指定行处插入一行
	*参数：row:插入的起始点（可视行号）
	*返回值：void
	*************************************************************************/
	virtual void	insertRow(int row) = 0;

	/*************************************************************************
	*函数名：insertRows
	*功能：在指定位置插入多行
	*参数：iPos，插入起始点，nCount:插入的行数（可视iPos）
	*返回值：void
	*************************************************************************/
	virtual void    insertRows(int iPos, int nCount) = 0;

	/*************************************************************************
	*函数名：removeColumn
	*功能：删除指定的列
	*参数：column:指定列
	*返回值：void
	*************************************************************************/
	virtual void	removeColumn(int column) = 0;

	/*************************************************************************
	*函数名：removeColumns
	*功能：删除指定列后count列
	*参数：column：列的位置,count：列数
	*返回值：void
	*************************************************************************/
	virtual void	removeColumns(int column, int count) = 0;

	/*************************************************************************
	*函数名：removeRow
	*功能：删除指定的行
	*参数：row：指定行（逻辑行号）
	*返回值：void
	*************************************************************************/
	virtual void	removeRow(int row) = 0;

	/*************************************************************************
	*函数名：removeRows
	*功能：删除指定行后count行
	*参数：row:行的位置,count:行数 （逻辑行号）
	*返回值：void
	*************************************************************************/
	virtual void	removeRows(int row, int count) = 0;

	/*************************************************************************
	*函数名：rowCount
	*功能： 获取表格的行数
	*参数：void
	*返回值：<0:有错，>=0:行的条数
	*************************************************************************/
	virtual int	    rowCount() const = 0;

	/*************************************************************************
	*函数名：columnCount
	*功能：获取表格的列数
	*参数：void
	*返回值：<0:有错，>=0:列的条数
	*************************************************************************/
	virtual int	    columnCount() const = 0;

	/*************************************************************************
	*函数名：setMaxRowCount
	*功能：设置表格的最大行数
	*参数：rows：行数
	*返回值：void
	*************************************************************************/
	virtual void	setMaxRowCount (int rows) = 0;

	/*************************************************************************
	*函数名：fastClearTable
	*功能：快速删除表格，删除后需要重新定义表头，设置行列数才能初始化表格
	*参数：void
	*返回值：void
	*************************************************************************/
	virtual void    fastClearTable() = 0;

	/*************************************************************************
	*函数名：beginbatchInsertRows
	*功能：开始插入多行
	*参数：void
	*返回值：void
	*************************************************************************/
	virtual void    beginbatchInsertRows() = 0;

	/*************************************************************************
	*函数名：endbatchInsertRows
	*功能：终止插入多行
	*参数：void
	*返回值：void
	*************************************************************************/
	virtual void    endbatchInsertRows() = 0;

	//add by jx -20150813
	/*************************************************************************
	*函数名：setFixPrecisonColumn
	*功能：设置固定精度的列
	*参数：columnArr：列号数组  nCnt：长度
	*返回值：void
	*************************************************************************/
	virtual void    setFixPrecisonColumn(int *arrColumn, int nCnt) = 0;
	
	//add by jx-20151022 happy
	/*************************************************************************
	*函数名：setEmptyItemComparable
	*功能：设置空单元格是否参与排序
	*参数：flag：1参与排序 0不参与排序
	*返回值：void
	*************************************************************************/
	virtual void	setEmptyItemComparable(bool flag = true) = 0;

	//add by yhh-20140709
	/*************************************************************************
	*函数名：setItemData
	*功能：设置单元格数据
	*参数：row：行号，column：列号，value：值（逻辑行号、列号）
	        precision:精度，默认-1不改变精度，精度设置只对浮点数有效（float和double） add 20150811
	*返回值：void
	*************************************************************************/
	virtual void    setItemData(int row, int column, INT64 value, bool bRefresh = false) = 0;
	virtual void    setItemData(int row, int column, int value, bool bRefresh = false) = 0;		//modify by yhh 20160315  "long" to "int"
	virtual void    setItemData(int row, int column, double value, int precision = -1, bool bRefresh = false) = 0;
	virtual void    setItemData(int row, int column, float value, int precision = -1, bool bRefresh = false) = 0;
	virtual void    setItemData(int row, int column, const char* pText, bool bRefresh=false) = 0;
	virtual void	setItemUserData(int row, int column, long value) = 0;	//通过该函数设置主键列的键值，与setPrimaryKey配合使用
	
	/*************************************************************************
	*函数名：refreshAll
	*功能：刷新显示表格
	*参数：void
	*返回值：void
	*************************************************************************/
	virtual void	refreshAll() = 0;
	
	/*************************************************************************
	*函数名：refreshRow
	*功能：刷新显示表格的指定行
	*参数：rowIndex：行号（可视行号）
	*返回值：void
	*************************************************************************/
	virtual void	refreshRow(int rowIndex) = 0;
	
	/*************************************************************************
	*函数名：refreshAll
	*功能：刷新显示表格指定区间的多行
	*参数：rowFrom：行起始号，rowTo：行结束号（可视行号）
	*返回值：void
	*************************************************************************/
	virtual void	refreshRows(int rowFrom, int rowTo) = 0;

	/*************************************************************************
	*函数名：setItemIcon
	*功能：设置元素图标
	*参数：row：行的位置, column：列的位置, pIcon:图标（逻辑行号、列号）
	*返回值：void
	*************************************************************************/
	virtual void    setItemIcon(int row, int column, IIcon* pIcon, bool bRefresh=false) = 0;
	
	/*************************************************************************
	*函数名：setItemColor
	*功能：设置单元格元素颜色（前景色）
	*参数：row；行的位置, column：列的位置, color：#RRGGBB 颜色（逻辑行号、列号）
	*返回值：void
	*************************************************************************/
	virtual void    setItemColor(int row, int column, long color, bool bRefresh=false) = 0;
    
	/*************************************************************************
	*函数名：setItemBackground
	*功能：设置单元格背景颜色
	*参数：row；行的位置, column：列的位置, color：#RRGGBB 颜色（逻辑行号、列号）
	*返回值：void
	*************************************************************************/
	virtual void    setItemBackground(int row, int column, long color, bool bRefresh=false) = 0;

	//add 20151022
	/*************************************************************************
	*函数名：setRowBackground
	*功能：设置行元素颜色（前景色）
	*参数：row；行的位置, column：列的位置, color：#RRGGBB 颜色（逻辑行号）
	*返回值：void
	*************************************************************************/
	virtual void    setRowBackground(int row, long color, bool bRefresh=false) = 0;

	/*************************************************************************
	*函数名：setRowTextColor
	*功能：设置行背景颜色
	*参数：row；行的位置, color：#RRGGBB 颜色（逻辑行号）
	*返回值：void
	*************************************************************************/
	virtual void    setRowTextColor(int row, long color, bool bRefresh=false) = 0;

	/*************************************************************************
	*函数名：setColumnText
	*功能：设置列头的标签文本
	*参数：column：列号, pText：文本（逻辑列号）
	*返回值：void
	*************************************************************************/
	virtual void    setColumnText(int column, const char* pText, bool bRefresh=false) = 0;
	
	/*************************************************************************
	*函数名：setColumnIcon
	*功能：设置某列的图标
	*参数：column：列号, pIcon：图标（逻辑列号）
	*返回值：void
	*************************************************************************/
	virtual void    setColumnIcon(int column, IIcon* pIcon, bool bRefresh=false) = 0;
	
	/*************************************************************************
	*函数名：setColumnColor
	*功能：设置某列的颜色
	*参数：column：列号, color：#RRGGBB 颜色（逻辑列号）
	*返回值：void
	*************************************************************************/
	virtual void    setColumnColor(int column, long color, bool bRefresh=false) = 0;
    
	/*************************************************************************
	*函数名：setColumnBackground
	*功能：设置某列的背景颜色
	*参数：column：列号, color：#RRGGBB 颜色
	*返回值：void
	*************************************************************************/
	//virtual void    setColumnBackground(int row, int column, long color, bool bRefresh=false) = 0;

	/*************************************************************************
	*函数名：getItemIntData
	*功能：得到某单元格的整型数据
	*参数：row：行号，column：列号  （逻辑行号、列号）
	*返回值：long型数据
	*************************************************************************/
	virtual long		getItemIntData(int row, int column) = 0;

	/*************************************************************************
	*函数名：getItemFloatData
	*功能：得到某单元格的浮点型数据
	*参数：row：行号，column：列号（逻辑行号、列号）
	*返回值：float型数据
	*************************************************************************/
	virtual float		getItemFloatData(int row, int column) = 0;

	/*************************************************************************
	*函数名：getItemStringData
	*功能：得到某单元格的字符串数据
	*参数：row：行号，column：列号（逻辑行号、列号）
	*返回值：HrString型数据
	*************************************************************************/
	virtual HrString   	getItemStringData(int row, int column) = 0;
	
	/*************************************************************************
	add by yuanx 20170111 增加获取item单元格的借口
	*函数名：getItemBackground
	*功能：得到某单元格的字符串数据
	*参数：row：行号，column：列号（逻辑行号、列号）
	*返回值：long型背景颜色#AARRGGBB 目前没有用透明度 AA=0;
	*************************************************************************/
	virtual long			getItemBackground(int row, int column) = 0;
	/*************************************************************************
	*函数名：getColumnText
	*功能：获取列头文本
	*参数：column：列的位置（逻辑列号）
	*返回值：const char*
	*************************************************************************/
	virtual const string	getColumnText(int column) = 0;
	
	/*************************************************************************
	*函数名：getRowIndex
	*功能：根据主键值priKey获取对应的行号
	*参数：priKey：主键值
	*返回值：const long（逻辑行号）
	*************************************************************************/
	virtual const long  getRowIndex(const long priKey) = 0;	//返回-1为失败

	/****add by hef 20190117*********************************************************************
	*函数名：getItemUserData
	*功能：得到某单元格的User数据
	*参数：row：行号，column：列号（逻辑行号、列号）
	*返回值：long型数据
	*************************************************************************/
	virtual long getItemUserData(int row, int column) = 0;

	/*************************************************************************
	*函数名：setPrimaryKey
	*功能：设置colIndex所在列为主键
	*参数：colIndex：列号
	*返回值：void
	*************************************************************************/
	virtual void	setPrimaryKey(const int colIndex=0) = 0;

	/*************************************************************************
	*函数名：setCheckState
	*功能： 设置检测状态
	*参数： row：行的位置, column：列的位置 , state：状态
	*返回值：void
	*************************************************************************/
	virtual void	setCheckState(int row, int column, ItemCheckState state, bool bRefresh=false) = 0;
	
	/*************************************************************************
	*函数名：setCheckable
	*功能：设置指定单元格是否勾选
	*参数：row：行的位置 , column：列的位置, checkable：是/否可检测
	*返回值：void
	*************************************************************************/
	virtual void	setCheckable(int row, int column, bool checkable) = 0;
	
	/*************************************************************************
	*函数名：setEditable
	*功能：设置指定单元格是否可编辑
	*参数：row：行的位置, column：列的位置, editable：是/否可编辑
	*返回值：void
	*************************************************************************/
	virtual void	setEditable(int row, int column, bool editable) = 0;
	
	/*************************************************************************
	*函数名：setEnabled
	*功能：设置指定单元格是否能够操作
	*参数：row:行的位置 , column：列的位置, enabled：是/否可操作的
	*返回值：void
	*************************************************************************/
	virtual void	setEnabled(int row, int column, bool enabled) = 0;

	/*************************************************************************
	*函数名：isCheckable
	*功能：判断单元格是否能够勾选
	*参数：单元格位置(row:行的位置, column：列的位置)
	*返回值：true:成功,false:失败
	*************************************************************************/
	virtual bool	isCheckable(int row, int column) const = 0;

	/*************************************************************************
	*函数名：isEditable
	*功能：判断单元格是否能够编辑
	*参数：单元格位置(row:行的位置, column：列的位置)
	*返回值：true:成功,false:失败
	*************************************************************************/
	virtual bool	isEditable(int row, int column) const = 0;

	/*************************************************************************
	*函数名：isEnable
	*功能：判断单元格是否能够操作
	*参数：单元格位置(row:行的位置, column：列的位置)
	*返回值：true:成功,false:失败
	*************************************************************************/
	virtual bool	isEnabled(int row, int column) const = 0;

	/*************************************************************************
	*函数名：getCheckState
	*功能：获取单元格的勾选状态
	*参数：单元格位置(row:行的位置, column：列的位置) 
	*返回值：ItemCheckState：勾选状态
	*************************************************************************/
	virtual ItemCheckState	getCheckState(int row, int column) const = 0;

	/*************************************************************************
	*函数名：setDynamicSortFilter
	*功能：设置动态排序过滤使能
	*参数：enable：动态排序过滤使能, true:可动态排序，false：不可动态排序
	*返回值：void
	*************************************************************************/
	virtual void	setDynamicSortFilter(bool enable) = 0;

	/*************************************************************************
	*函数名：dynamicSortFIlter
	*功能：获取动态排序过滤使能值
	*参数：void
	*返回值：true:可动态排序，false：不可动态排序
	*************************************************************************/
	virtual bool	dynamicSortFilter() const = 0;

	/*************************************************************************
	*函数名：invalidateFilter
	*功能：刷新表格的排序过滤
	*参数：void
	*返回值：void
	*************************************************************************/
	virtual void	invalidateFilter() = 0;

	/*************************************************************************
	*函数名：setComboBoxDelegateForColumn
	*功能：设置指定列的ComboBoxDelegate代理
			（仅在单元格为可编辑状态下有效，调用setEditable设置即可）
	*参数：column：列号, strItemList：ComboBox的下拉元素值（可视列号）
	*返回值：void
	*************************************************************************/
	virtual void    setComboBoxDelegateForColumn(int column, StringList& strItemList) = 0;
	
	/*************************************************************************
	*函数名：setSpinBoxDelegateForColumn
	*功能：设置指定列的SpinBoxDelegate代理
			（仅在单元格为可编辑状态下有效，调用setEditable设置即可）
	*参数：column：列号, MinVal：最小值，MaxVal：最大值（可视列号）
	*返回值：void
	*************************************************************************/
	virtual void    setSpinBoxDelegateForColumn(int column, int MinVal, int MaxVal) = 0;

	/*add by hejy 20170227****************************************************
	*函数名：setDoubleSpinBoxDelegateForColumn
	*功能：设置指定列的DoubleSpinBoxDelegate代理
			（仅在单元格为可编辑状态下有效，调用setEditable设置即可）
	*参数：column：列号（可视列号）, MinVal：最小值，MaxVal：最大值，DecNum：小数精度
	*返回值：void
	*************************************************************************/
	virtual void    setDoubleSpinBoxDelegateForColumn(int column, double MinVal, double MaxVal, int DecNum = 2) = 0;

	/*************************************************************************
	*函数名：setLineEditDelegateForColumn
	*功能：设置指定列的LineEditDelegate代理
			（仅在单元格为可编辑状态下有效，调用setEditable设置即可）
	*参数：column：列号（可视列号）, pValidator:输入文本的合法校验
	*返回值：void
	*************************************************************************/
	virtual void    setLineEditDelegateForColumn(int column, const QValidator *pValidator = NULL) = 0;		//add by yhh 20190625

	/*************************************************************************
	*函数名：setDateTimeEditDelegateForColumn
	*功能：设置指定列的DateTimeEditDelegate代理
			（仅在单元格为可编辑状态下有效，调用setEditable设置即可）
	*参数：column：列号（可视列号）
	*返回值：void
	*************************************************************************/
	virtual void    setDateTimeEditDelegateForColumn(int column, string strFomat) = 0;

	/*add by yuanx 20170110***************************************************
	*函数名：setBackGroundRectDelegateForColumn
	*功能：设置指定列的BackGroundRectDelegate代理
			（仅在单元格为可编辑状态下有效，调用setEditable设置即可）
	*参数：column：列号（可视列号）
	*返回值：void
	*************************************************************************/
	virtual void    setBackGroundRectDelegateForColumn(int column) = 0;

	/*************************************************************************
	*函数名：setTableObserver
	*功能：设置表格的观察者
	*参数：pObserver:观察者指针
	*返回值：void
	*************************************************************************/
	virtual void setTableObserver(ITableObserver* pObserver) = 0;

	/*************************************************************************
	*函数名：getTableObserver
	*功能：获得表格的观察者
	*参数：void
	*返回值：ITableObserver*：观察者指针
	*************************************************************************/
	virtual ITableObserver* getTableObserver() = 0;

	/*************************************************************************
	*函数名：setDynamicFillterSink
	*功能：设置表格的排序过滤钩子
	*参数：IDynamicFillterSink*：过滤钩子指针
	*返回值：void
	*************************************************************************/
	virtual void setDynamicFillterSink(IDynamicFillterSink* pSink) = 0;
	
	/*************************************************************************
	*函数名：getDynamicFillterSink
	*功能：获得表格的排序过滤钩子接口
	*参数：void
	*返回值：IDynamicFillterSink*：过滤钩子指针
	*************************************************************************/
	virtual IDynamicFillterSink* getDynamicFillterSink() = 0;

	
	/*********************HeaderView相关函数**********************************
	
	//add by jx 20150420
	/*************************************************************************
	*函数名：getAllColumnText
	*功能：获取列头的标签文本
	*参数：disCnt 可见的列数
	*返回值：string 可见的列名，以,号隔开
	*************************************************************************/
	virtual string getAllColumnText(int &disCnt) = 0;

	/*************************************************************************
	*函数名：setHeaderSectionHidden
	*功能：设置表头指定逻辑字段的显隐
	*参数：logicalIndex：逻辑字段号, hide：隐藏/不可见
	*返回值：void
	*************************************************************************/
	virtual void		setHeaderSectionHidden(int logicalIndex, bool hide) = 0;

	/*************************************************************************
	*函数名：setHeaderClickable
	*功能：设置表头单击响应使能
	*参数：clickable为true:响应鼠标单击, false:不响应鼠标单击
	*返回值：void
	*************************************************************************/
	virtual void		setHeaderClickable(bool clickable) = 0;

	/*************************************************************************
	*函数名：moveHeaderSection
	*功能：移动表头字段从"from"位置到"to"位置
	*参数：from："from"位置 ，to："to"位置
	*返回值：void
	*************************************************************************/
	virtual void		moveHeaderSection(int from, int to) = 0;

	/*************************************************************************
	*函数名：getHeaderVisualIndex
	*功能：根据逻辑索引列号获得对应的可视索引列号
	*参数：logicalIndex:逻辑索引列号
	*返回值：可视索引列号
	*************************************************************************/
	virtual int			getHeaderVisualIndex(int logicalIndex) = 0;

	/*************************************************************************
	*函数名：getHeaderLogicalIndex
	*功能：根据可视索引列号获得对应的逻辑索引列号
	*参数：visualIndex：可视索引列号
	*返回值：逻辑索引列号
	*************************************************************************/
	virtual int			getHeaderLogicalIndex(int visualIndex) = 0;

	/*add by hejy 20170227****************************************************
	*函数名：setCellWidget
	*功能：给单元格设置显示的窗口指针
	*参数：row：行号，column：列号，widget：窗口指针
	*返回值：bool，true:成功，false：失败
	*************************************************************************/
	virtual bool	setCellWidget(int row, int column, void * widget ) = 0;
};
#endif
