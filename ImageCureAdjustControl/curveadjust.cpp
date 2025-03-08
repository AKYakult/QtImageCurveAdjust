#include "curveadjust.h"

#include "internal/aaCurve.h"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRect>

CurveAdjust::CurveAdjust(QWidget *parent) :QWidget(parent)
{
    setFixedSize(256 + m_margin,256 + m_margin);
}

bool CurveAdjust::init(const QImage& img)
{
    m_channel = ICAChannel::eChannelAll;
    m_channelMode = ICAChannelMode::eChannelModeAll;

    m_channel2Knots.clear();
    m_channel2CurveData.clear();

    m_curCurve = QPainterPath();

    m_curKnotIndex = -1;
    m_curDragKnotIndex = -1;
    m_noMove2Del = false;
    m_hasMove = false;

    if (img.isNull()) return false;

    QImage::Format fmt = img.format();
    qDebug()<<"noe img format:"<<fmt;

    switch (fmt)
    {
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
        //
        break;
    case QImage::Format_Grayscale8:
        //        break;
    default:
        return false;//格式不支持
    }

    std::list<ICAChannel> channels;
    channels.push_back(ICAChannel::eChannelAll);
    if(fmt != QImage::Format_Grayscale8)
    {
        channels.push_back(ICAChannel::eChannelR);
        channels.push_back(ICAChannel::eChannelG);
        channels.push_back(ICAChannel::eChannelB);
    }

    for(auto channel:channels)
    {
        //曲线起始控制点
        auto& knots = m_channel2Knots[channel];
        knots.push_back(std::pair<unsigned char, unsigned char>(0, 0));
        knots.push_back(std::pair<unsigned char, unsigned char>(255, 255));

        //计算曲线数据
        CalcCurveData(channel);
    }

    return true;
}

void CurveAdjust::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    QPen pen;

    ICAChannel currentChannel = GetCurrentChannel(m_channelMode);

    QRect drawArea = getDrawArea();
    //基线
    if (base_line)
    {
        // 绘制边框（可选）
        QPen framePen(Qt::darkGray);
        painter.setPen(framePen);
        painter.drawRect(drawArea);

        // 绘制对角线（基线）
        QPen pen(Qt::gray);
        painter.setPen(pen);
        //        painter.drawLine(drawArea.left(), drawArea.bottom(),
        //                         drawArea.right(), drawArea.top());

        painter.drawLine(drawArea.bottomLeft(),drawArea.topRight());
    }

    //根据rgb来绘制不同颜色
    auto GetChannelColor = [](ICAChannel channel)->QColor
    {
        switch (channel)
        {
        case ICAChannel::eChannelAll:
            return QColor(Qt::darkGreen);
            break;
        case ICAChannel::eChannelR:
            return QColor(Qt::red);
            break;
        case ICAChannel::eChannelG:
            return QColor(Qt::green);
            break;
        case ICAChannel::eChannelB:
            return QColor(Qt::blue);
            break;
        default:
            break;
        }
        Q_ASSERT(false);
        return QColor(Qt::black);
    };

    // 绘制曲线
    QList<ICAChannel> chs;
    currentChannel == ICAChannel::eChannelAll ? (chs << ICAChannel::eChannelAll) :
        (chs << ICAChannel::eChannelR << ICAChannel::eChannelG << ICAChannel::eChannelB);

    for (auto channel : chs)
    {
        if(channel != currentChannel && !bCombineShow) continue;

        auto& curveData = m_channel2CurveData[channel];
        int sz = curveData.size();
        if (sz > 1)
        {
            QColor color = GetChannelColor(channel);
            pen.setColor(color);
            painter.setPen(pen);
            QPainterPath curCurvePath;
            auto it = curveData.begin();
            curCurvePath.moveTo(CoordinatePos2WndPos(QPoint(it->first,it->second)));
            ++it;
            while(it!= curveData.end())
            {
                curCurvePath.lineTo(CoordinatePos2WndPos(QPoint(it->first, it->second)));
                ++it;
            }
            painter.drawPath(curCurvePath);
            if(channel == currentChannel) m_curCurve = curCurvePath;
        }
    }

    // 绘制控制点
    auto &knotsData = m_channel2Knots[currentChannel];
    int sz = knotsData.size();
    if (sz > 0)
    {
        Q_ASSERT(sz >= 2);
        QColor color = GetChannelColor(currentChannel);
        pen.setColor(color);
        painter.setPen(pen);

        for (int i = 0; i < sz; ++i)
        {
            QRect rect = GetKnotRect(knotsData[i].first, knotsData[i].second);
            painter.drawRect(rect);
            if (i == m_curKnotIndex)
            {
                painter.fillRect(rect, color);
            }
        }
    }

}


/*
void CurveAdjust::mousePressEvent(QMouseEvent *event)
{

    if (event->buttons() == Qt::MouseButton::RightButton)//右键删除控制点
    {
        auto outPos = WndPos2CoordinatePos(event->pos());
        if (outPos != QPoint(-1, -1))
        {
            m_curKnotIndex = -1;
            ICAChannel currentChannel = GetCurrentChannel(m_channelMode);
            auto& knotsData = m_channel2Knots[currentChannel];
            //掐头去尾查找   不删除头尾两个点
            auto it = knotsData.begin()+1;
            bool finded = false;
            for (;it!= knotsData.end() -1 ;)
            {
                QPoint knotPos(it->first, it->second);
                QRect rect = GetKnotRect(knotPos.x(),knotPos.y());
                if (rect.contains(event->pos()))
                {
                    knotsData.erase(it);
                    finded = true;
                    break;
                }
                ++it;
            }
            if (finded)
            {
                CalcCurveData(currentChannel);
                repaint();
            }
        }
    }else if (event->buttons() == Qt::MouseButton::LeftButton)//左键添加控制点并开启拖拽
    {
        auto outPos = WndPos2CoordinatePos(event->pos());
        if (outPos != QPoint(-1, -1))
        {
            m_curKnotIndex = -1;
            //先判定是否点到了已有控制点  同时查找新插入的下标
            int newIndex = -1;
            ICAChannel currentChannel = GetCurrentChannel(m_channelMode);
            auto& knotsData = m_channel2Knots[currentChannel];
            int sz = knotsData.size();
            for (int i = 0; i< sz; ++i)
            {
                QRect rect = GetKnotRect(knotsData[i].first, knotsData[i].second);
                if (rect.contains(event->pos()) && m_curKnotIndex == -1)
                {
                    m_curDragKnotIndex = i;
                    m_curKnotIndex = i;
                    m_noMove2Del = false;
                }

                if (knotsData[i].first < outPos.x())
                {
                    newIndex = i + 1;
                }
            }
            //找不到则继续判定是否要添加新的控制点
            if (m_curDragKnotIndex == -1)
            {
                if (newIndex > 0 && newIndex < sz)
                {
                    QPainterPath testArea;
                    auto pos = QPoint(event->pos().x() - m_test_des, event->pos().y() - m_test_des);
                    testArea.addEllipse(pos.x(), pos.y(), knotSize, knotSize);
                    //测试鼠标是否接近曲线
                    if (testArea.intersects(m_curCurve))
                    {
                        int x1 = knotsData[newIndex - 1].first;
                        int x2 = knotsData[newIndex].first;
                        if (outPos.x() > (x1 + knotSize) && outPos.x() < (x2 - knotSize))
                        {
                            auto it = knotsData.begin() + newIndex;
                            knotsData.insert(it, std::pair<unsigned char, unsigned char>(outPos.x(), outPos.y()));
                            m_curDragKnotIndex = newIndex;
                            m_noMove2Del = true;
                        }
                    }
                    else
                    {
                        repaint();
                    }
                }
                else
                {
                    repaint();
                }
            }
            else//找到则更新界面刷新当前控制点显示
            {
                repaint();
            }
        }
    }

    return QWidget::mousePressEvent(event);
}

void CurveAdjust::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MouseButton::LeftButton)
    {
        //停止拖拽
        if (m_curDragKnotIndex != -1)
        {
            //处理新添加但是没动过的控制点直接删除
            if (m_noMove2Del && !m_hasMove)
            {
                ICAChannel currentChannel = GetCurrentChannel(m_channelMode);
                auto& knotsData = m_channel2Knots[currentChannel];
                auto it = knotsData.begin() + m_curDragKnotIndex;
                knotsData.erase(it);
            }

            //动过控制点的发送信号
            if (m_hasMove)
            {
                //
            }

            m_curDragKnotIndex = -1;
        }
        m_hasMove = false;
    }
    return QWidget::mouseReleaseEvent(event);
}

void CurveAdjust::mouseMoveEvent(QMouseEvent *event)
{
    auto outPos = WndPos2CoordinatePos(event->pos());
    if (outPos != QPoint(-1, -1))
    {

        //设置界面显示坐标系的值
        //        ui->label_inNum->setText(QString::number(outPos.x()));
        //        ui->label_outNum->setText(QString::number(outPos.y()));

        qDebug()<<"now    in:"<<QString::number(outPos.x());
        qDebug()<<"now out:"<<QString::number(outPos.y());

        //处理拖拽
        if (event->buttons() == Qt::MouseButton::LeftButton && m_curDragKnotIndex != -1)
        {
            ICAChannel currentChannel = GetCurrentChannel(m_channelMode);
            if (TestNewPos(outPos,currentChannel,m_curDragKnotIndex))
            {
                m_hasMove = true;//有效的移动才标记为真
                auto& knotsData = m_channel2Knots[currentChannel];
                knotsData[m_curDragKnotIndex].first = outPos.x();
                knotsData[m_curDragKnotIndex].second = outPos.y();
                CalcCurveData(currentChannel);
                repaint();
            }
        }


    }
    else
    {
        //        ui->label_inNum->clear();
        //        ui->label_outNum->clear();
    }

    return QWidget::mouseMoveEvent(event);
}
*/


void CurveAdjust::mousePressEvent(QMouseEvent* event)
{
    if (event->buttons() == Qt::MouseButton::RightButton) // 右键删除控制点
    {
        auto outPos = WndPos2CoordinatePos(event->pos());
        qDebug()<<"click 2 coordinate pos:"<<outPos;
        if (outPos != QPoint(-1, -1))
        {
            m_curKnotIndex = -1;

            ICAChannel currentChannel = GetCurrentChannel(m_channelMode);

            auto& knotsData = m_channel2Knots[currentChannel];
            // 掐头去尾查找 不删除头尾两个点
            auto it = knotsData.begin() + 1;
            for (int i = 1; i < knotsData.size() - 1; ++i, ++it)
            {
                QRect rc = GetKnotRect(it->first, it->second);
                if (rc.contains(event->pos()))
                {
                    knotsData.erase(it);
                    CalcCurveData(currentChannel);
                    update();
                    break;
                }
            }
        }
    }
    else if (event->buttons() == Qt::MouseButton::LeftButton) // 左键选择或添加控制点
    {
        auto outPos = WndPos2CoordinatePos(event->pos());
        if (outPos != QPoint(-1, -1))
        {
            ICAChannel currentChannel = GetCurrentChannel(m_channelMode);
            auto& knotsData = m_channel2Knots[currentChannel];

            // 先查找是否点击了已有的控制点
            int index = FindNearestKnot(event->pos(), currentChannel);
            if (index >= 0)
            {
                // 选中已有控制点
                m_curKnotIndex = index;
                m_curDragKnotIndex = index;
                m_hasMove = false;
                update();
            }
            else
            {
                // 添加新控制点
                bool bInserted = false;
                for (int i = 0; i < knotsData.size() - 1; ++i)
                {
                    if (outPos.x() > knotsData[i].first && outPos.x() < knotsData[i + 1].first)
                    {
                        // 检查与相邻点的最小距离
                        int minDistance = knotSize * 2; // 增加最小距离要求
                        if (outPos.x() - knotsData[i].first > minDistance &&
                            knotsData[i + 1].first - outPos.x() > minDistance)
                        {
                            // 在两个控制点之间插入新点
                            knotsData.insert(knotsData.begin() + i + 1, std::pair<unsigned char, unsigned char>(outPos.x(), outPos.y()));
                            m_curKnotIndex = i + 1;
                            m_curDragKnotIndex = i + 1;
                            m_hasMove = false;
                            bInserted = true;
                            CalcCurveData(currentChannel);
                            update();
                            break;
                        }
                    }
                }

                if (!bInserted)
                {
                    // 如果没有找到合适的位置 可能是不符合规则
                    m_curKnotIndex = -1;
                    m_curDragKnotIndex = -1;
                }
            }
        }
    }
}

void CurveAdjust::mouseMoveEvent(QMouseEvent* event)
{
    if (m_curDragKnotIndex >= 0 && event->buttons() == Qt::MouseButton::LeftButton)
    {
        auto outPos = WndPos2CoordinatePos(event->pos());
        if (outPos != QPoint(-1, -1))
        {
            ICAChannel currentChannel = GetCurrentChannel(m_channelMode);

            // 测试新位置是否有效
            if (TestNewPos(outPos, currentChannel, m_curDragKnotIndex))
            {
                auto& knotsData = m_channel2Knots[currentChannel];
                knotsData[m_curDragKnotIndex].first = outPos.x();
                knotsData[m_curDragKnotIndex].second = outPos.y();
                m_hasMove = true;
                CalcCurveData(currentChannel);
                update();
            }
        }
    }
}

void CurveAdjust::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_curDragKnotIndex >= 0)
    {
        // 如果设置了m_noMove2Del并且没有移动，可以在这里处理删除逻辑
        if (m_noMove2Del && !m_hasMove)
        {
            ICAChannel currentChannel = GetCurrentChannel(m_channelMode);
            auto& knotsData = m_channel2Knots[currentChannel];

            // 不删除首尾两个点
            if (m_curDragKnotIndex > 0 && m_curDragKnotIndex < knotsData.size() - 1)
            {
                knotsData.erase(knotsData.begin() + m_curDragKnotIndex);
                CalcCurveData(currentChannel);
                m_curKnotIndex = -1;
                update();
            }
        }

        m_curDragKnotIndex = -1;
        m_hasMove = false;
    }
}

void CurveAdjust::CalcCurveData(ICAChannel channel)
{
    auto const & srcData = m_channel2Knots[channel];
    aaAaa::aaCurvePtr pCurve = aaAaa::aaCurveFactory::createCurve(srcData,aaAaa::SplineType::SPLINE_CUBIC);
    int s = srcData.begin()->first;
    int e = srcData.rbegin()->first;
    auto& curveData = m_channel2CurveData[channel];
    curveData.clear();
    for (int i=s;i<=e;++i)
    {
        double y = 0;
        pCurve->getValue(i, y);
        unsigned char y1 = 0;
        if (y <= 255 && y >= 0) y1 = y;
        else if (y > 255) y1 = 255;
        else if (y < 0) y1 = 0;
        curveData.push_back(std::pair<unsigned char, unsigned char>(i, y1));
    }
}

QRect CurveAdjust::getDrawArea() const
{
    int drawAreaLeft = m_margin;
    int drawAreaTop = m_margin;
    int drawAreaRight = width() - m_margin;
    int drawAreaBottom = height() - m_margin;

    return QRect(drawAreaLeft, drawAreaTop,
                 drawAreaRight - drawAreaLeft,
                 drawAreaBottom - drawAreaTop);
}

QPoint CurveAdjust::CoordinatePos2WndPos(QPoint pos)
{
    QRect drawArea = getDrawArea();
    int x = pos.x() * (drawArea.width() - 1) / 255 + drawArea.left();
    int y = drawArea.bottom() - pos.y() * (drawArea.height() - 1) / 255;
    return QPoint(x, y);
}

QPoint CurveAdjust::WndPos2CoordinatePos(QPoint pos)
{
    QRect drawArea = getDrawArea();
    if (!drawArea.contains(pos))
        return QPoint(-1, -1);

    int x = (pos.x() - drawArea.left()) * 255 / (drawArea.width() - 1);
    int y = (drawArea.bottom() - pos.y()) * 255 / (drawArea.height() - 1);

    if (x >= 0 && y >= 0 && x <= 255 && y <= 255)
        return QPoint(x, y);

    return QPoint(-1, -1);
}

QRect CurveAdjust::GetKnotRect(int x, int y)
{
    QPoint wndPos = CoordinatePos2WndPos(QPoint(x, y));
    int x1 = wndPos.x() - (knotSize / 2);
    int y1 = wndPos.y() - (knotSize / 2);
    return QRect(x1, y1, knotSize, knotSize);
}

bool CurveAdjust::TestNewPos(QPoint pos, ICAChannel channel, int index)
{
    auto const & knotsData = m_channel2Knots[channel];
    int i1 = (index == 0 ? 0 : index - 1);
    int i2 = (index == (knotsData.size() - 1) ? index : index + 1);
    int x1 = knotsData[i1].first;
    int x2 = knotsData[i2].first;

    // 确保控制点不会越过相邻控制点
    int offset = knotSize*2;
    if ((pos.x() > (x1 + offset) || index == 0) &&
        (pos.x() < (x2 - offset) || index == (knotsData.size() - 1)))
    {
        return true;
    }
    return false;
}

int CurveAdjust::FindNearestKnot(QPoint pos, ICAChannel channel)
{
    auto const & knotsData = m_channel2Knots[channel];
    for (int i = 0; i < knotsData.size(); ++i)
    {
        QRect rc = GetKnotRect(knotsData[i].first, knotsData[i].second);
        if (rc.contains(pos))
        {
            return i;
        }
    }
    return -1;
}

CurveAdjust::ICAChannel CurveAdjust::GetCurrentChannel(ICAChannelMode mode)
{
    ICAChannel currentChannel = ICAChannel::eChannelAll;
    switch (mode) {
    case ICAChannelMode::eChannelModeAll:{
        currentChannel = ICAChannel::eChannelAll;
        break;
    }
    default:{
        currentChannel = m_channel;
        break;
    }
    }

    return currentChannel;
}
