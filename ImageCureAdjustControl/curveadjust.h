#ifndef CURVEADJUST_H
#define CURVEADJUST_H

#include <QObject>
#include <QWidget>
#include <QMap>
#include <vector>
#include <utility>  // 添加此行以支持 std::pair

#include <QPainterPath>

//#include "ImageGlobalDef.h"

class CurveAdjust :public QWidget
{
    Q_OBJECT
public:
    explicit CurveAdjust(QWidget *parent = nullptr);

    bool init(const QImage& img);


protected:
    virtual void paintEvent(QPaintEvent* event)override;
    virtual void mousePressEvent(QMouseEvent* event)override;
    virtual void mouseReleaseEvent(QMouseEvent* event)override;
    virtual void mouseMoveEvent(QMouseEvent* event)override;

private:
    //曲线调整模式
    enum class ICAChannelMode:int
    {
        eChannelModeAll,//灰度图使用，或彩色图使用是RGB整体调整
        eChannelModeSingle,//单通道调整（彩色图使用）
        eChannelModeMulti,//多通道调整（彩色图使用）
    };

    //曲线调整的色彩通道
    enum class ICAChannel:int
    {
        eChannelAll,//灰度图使用，或彩色图使用是RGB整体调整
        eChannelR,//红色通道
        eChannelG,//绿色通道
        eChannelB,//蓝色通道
    };

    ICAChannel m_channel = ICAChannel::eChannelAll;//通道

    ICAChannelMode m_channelMode = ICAChannelMode::eChannelModeAll;//类型

    QMap<ICAChannel
         , std::vector<std::pair<unsigned char, unsigned char> > > m_channel2Knots;//控制点

    QMap<ICAChannel
         , std::list<std::pair<unsigned char, unsigned char> > > m_channel2CurveData;//曲线数据


    QPainterPath m_curCurve;

private:
    bool base_line = true;
    int m_margin = 20;
    int knotSize = 5;
    bool bCombineShow = false;

    //-------------------

    int m_test_des = 4;
    int m_curKnotIndex = -1;
    int m_curDragKnotIndex = -1;
    bool m_noMove2Del = false;
    bool m_hasMove = false;

private:
    //样条插值计算得到的一系列点
    void CalcCurveData(ICAChannel channel);

    QRect getDrawArea() const;

    // 坐标系转换：曲线坐标转窗口坐标
    QPoint CoordinatePos2WndPos(QPoint pos);

    // 坐标系转换：窗口坐标转曲线坐标
    QPoint WndPos2CoordinatePos(QPoint pos);

    // 获取控制点矩形
    QRect GetKnotRect(int x, int y);

    bool TestNewPos(QPoint pos, ICAChannel channel, int index);

    int FindNearestKnot(QPoint pos, ICAChannel channel);

    //获取当前的通道
    CurveAdjust::ICAChannel GetCurrentChannel(ICAChannelMode mode);

};

#endif // CURVEADJUST_H
