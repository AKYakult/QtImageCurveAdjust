#include "ImgAdjust.h"
#include <QApplication>
#include <QFileDialog>
#include <ImageCureAdjustControl/curveadjust.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    ImgAdjust w;
    w.show();

    QString filePath = QFileDialog::getOpenFileName(nullptr,"Image");
    QFile file(filePath);
    QImage m_img;
    if(file.open(QFile::ReadOnly))
    {
        QByteArray buf = file.readAll();
        if(m_img.loadFromData(buf))
        {
            if(m_img.format() == QImage::Format_Indexed8)
            {
                m_img = m_img.convertToFormat(QImage::Format_Grayscale8);
            }
        }
    }

    CurveAdjust c;
    c.init(m_img);
    c.show();

    return a.exec();
}
