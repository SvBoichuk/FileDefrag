#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <vector>
#include <windows.h>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    bool GetFileMapping();
    bool GetVolumeBitmap();
    bool MoveFileClusters();
    bool OpenExFile(HANDLE& _hFile);
    void DrawMap();
    void ReDrawMap();
    ~MainWindow();

private slots:
    void on_open_btn_clicked();

    void on_get_map_btn_clicked();

    void on_getVol_btn_clicked();

    void on_defrag_btn_clicked();

    void on_clear_btn_clicked();

    void on_pushButton_5_clicked();

private:
    Ui::MainWindow *ui;
    QString path; // шлях до файлу для QFileDialog
    HANDLE hFile;
    PRETRIEVAL_POINTERS_BUFFER OutBuf; // містить схему розташування файлу
    ULONG file_size; //розмір файлу
    ULONG SectorPerCl, BtPerSector; // к-ксть секторів у класі і розмір одного сектора
    ULONG ClusterSize; // розмір кластера
    ULONG ClusterCount; // к-ксть кластерів
    __int64 StartLCN;
    __int64 PrevVCN;

    std::vector<int> LCN_FILE;
    VOLUME_BITMAP_BUFFER* BitBuf;
};

#endif // MAINWINDOW_H
