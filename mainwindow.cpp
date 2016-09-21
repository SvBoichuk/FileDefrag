#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>
#include <string>
#include <stdexcept>
#include <exception>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("Simple file defrag");

    StartLCN = 0;
    BitBuf = NULL;
    ui->get_map_btn->setEnabled(false);
    ui->getVol_btn->setEnabled(false);
    ui->defrag_btn->setEnabled(false);

    ui->tableWidget->setRowCount(200);
    ui->tableWidget->setColumnCount(200);
}

bool MainWindow::GetFileMapping()
{
    file_size = GetFileSize(hFile, NULL);
    ui->listWidget->addItem("File size (in bytes): " + QString::number(file_size));

    std::string dir = path.toStdString().substr(0,3);

    // визначити розмір і к-сть секторів у кластері
    if ( GetDiskFreeSpaceA(dir.c_str(), &SectorPerCl, &BtPerSector, NULL, NULL) != 0)
    {
        ui->listWidget->addItem("Sector in cluster: " + QString::number(SectorPerCl));
        ui->listWidget->addItem("Sector size (in bytes): " + QString::number(BtPerSector));
        ClusterSize = SectorPerCl * BtPerSector;
        ui->listWidget->addItem("Cluster size: " + QString::number(ClusterSize));
    }
    else
    {
         ui->listWidget->addItem("Can't get disk free space!");
         ui->listWidget->addItem("Last error: " + QString::number(GetLastError()));
         return false;
    }

    //Наступний етап - отримати поточну схему розташування файлу на диску
    ULONG OutSize; // розмір структури для отримання розташування файлу
    OutSize = sizeof(RETRIEVAL_POINTERS_BUFFER) + (file_size / ClusterSize) * sizeof(OutBuf->Extents);

    ui->listWidget->addItem("Struct RTRIEVAL_POINTERS_BUFFER size: " + QString::number(OutSize));

    OutBuf = new RETRIEVAL_POINTERS_BUFFER[OutSize];

    STARTING_VCN_INPUT_BUFFER InBuf;
    InBuf.StartingVcn.QuadPart = 0; // !!!

    DWORD Bytes;

    if (DeviceIoControl(hFile, FSCTL_GET_RETRIEVAL_POINTERS, &InBuf, sizeof(InBuf), OutBuf, OutSize, &Bytes, NULL)) // !!!
    {
        ClusterCount = (file_size + ClusterSize - 1) / ClusterSize;
        ui->listWidget->addItem("Cluster count: " + QString::number(ClusterCount));
        ui->listWidget->addItem("Bytes (bufer data): " +QString::number(Bytes));
    }
    else {
       ui->listWidget->addItem("Error! Error code: " + QString::number( GetLastError() ) );
       return false;
    }

    PrevVCN = OutBuf->StartingVcn.QuadPart;
    ui->listWidget->addItem("Extents: " + QString::number(OutBuf->ExtentCount) );

    if(OutBuf->ExtentCount == 1)
    {
        ui->listWidget->addItem("File is defragmented!");
        QMessageBox::information(this,"Info","File is defragmented!!!");
        return true;
    }

    //показати лог.номер кластера
    ui->listWidget->addItem("*****LCN:*****");
    for (unsigned int i = 0; i < OutBuf->ExtentCount; i++)
    {
      ui->listWidget->addItem("LNC Quad(for 64bit): " + QString::number( OutBuf->Extents[i].Lcn.QuadPart ) );
      ui->listWidget->addItem("VNC: " + QString::number( OutBuf->Extents[i].NextVcn.QuadPart ));
      LCN_FILE.push_back(OutBuf->Extents[i].Lcn.QuadPart);
    }

    ui->listWidget->addItem("Extents lenght:");
    for(unsigned int i = 0; i < OutBuf->ExtentCount; i++)
    {
        ui->listWidget->addItem("extent #" + QString::number(i+1) + ": length: "
                                + QString::number(OutBuf->Extents[i].NextVcn.QuadPart - PrevVCN) );
        PrevVCN = OutBuf->Extents[i].NextVcn.QuadPart;
    }

    PrevVCN = OutBuf->StartingVcn.QuadPart;

    return true;
}

bool MainWindow::GetVolumeBitmap()
{
    std::string disk("\\\\.\\");                 // імя тому на якому розміщено файл
    disk.append(path.toStdString().substr(0,2)); // тут будемо шукати вільні кластери
    ui->listWidget->addItem( "Disk name: " + QString::fromStdString(disk) );

    /*Вхідні параметри*/
    STARTING_LCN_INPUT_BUFFER LCN_Buf;

    /*Отримати дескриптор тому*/
    HANDLE hDevice;
    hDevice = CreateFileA(disk.c_str(),GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,0,0);

    if(hDevice == INVALID_HANDLE_VALUE)
    {
        ui->listWidget->addItem("Error! Invalid handle value! Error code: " + QString::number(GetLastError()));
        return false;
    }
    else ui->listWidget->addItem("Open volume handle!");


    //викличемо ф-цію DeviceIoControl перший раз із дуже малим буфером
    //в BitBuf->BitmapSize запишеться загальне число кластерів
    //і ми зможемо виділити достатньо памяті під буфер щоб зберегти інформацію про кластери
    ULONG OutSize = sizeof(VOLUME_BITMAP_BUFFER) + 4;

    BitBuf = new VOLUME_BITMAP_BUFFER[OutSize];
    LCN_Buf.StartingLcn.QuadPart = 0;
    DWORD Bytes;
    int ret = 0;
    ret = DeviceIoControl(hDevice,FSCTL_GET_VOLUME_BITMAP,&LCN_Buf, sizeof(LCN_Buf),BitBuf,OutSize,&Bytes,NULL);
    /***********************************************************************************************************/

    ULONG CountEmptyClusters = 0;

    ui->listWidget->addItem("BitmapSize: " + QString::number(BitBuf->BitmapSize.QuadPart));
    if( (!ret) && GetLastError() == ERROR_MORE_DATA)
    {
        //отримати к-ксть кластерів на томі(починаючи з StartingLCN)
        ULONG ClusterCountOnVolume = BitBuf->BitmapSize.QuadPart - BitBuf->StartingLcn.QuadPart;
        ui->listWidget->addItem("Cluster count on volume: " + QString::number(ClusterCountOnVolume));
        // обчислити, скільки потрібно байт під буфер
        delete [] BitBuf;
        OutSize = sizeof(VOLUME_BITMAP_BUFFER) + (ClusterCountOnVolume / 8) + 1;
        BitBuf = new VOLUME_BITMAP_BUFFER[OutSize];
        if(!BitBuf)
        {
            ui->listWidget->addItem("Error! Can't alloc memory!");
            return false;
        }
        BitBuf->StartingLcn.QuadPart = 0; //буфер новий - задамо початок
        ret = DeviceIoControl(hDevice, FSCTL_GET_VOLUME_BITMAP, &LCN_Buf, sizeof(LCN_Buf), BitBuf,OutSize,&Bytes,NULL);
        if(ret)
        {
            ui->listWidget->addItem("BitmapSize #2: " + QString::number(BitBuf->BitmapSize.QuadPart));
        }
        else{
            ui->listWidget->addItem("Ret value: " + QString::number(ret));
            ui->listWidget->addItem("Last error code: " + QString::number(GetLastError()));
            return false;
        }

        //визначити послідовність вільних кластерів
        //достатніх для утримування файлу

        ULONG tempLCN;

        int InUse = 0;
        int Mask = 1;

        for(__int64 i = 0; i < (BitBuf->BitmapSize.QuadPart / 8) - 1; i++)
        {
            if(BitBuf->Buffer[i] == 255)
            {
                CountEmptyClusters = 0;
                StartLCN += 8;
                continue;
            }

            while(Mask != 256)
            {
                if (!CountEmptyClusters) {
                    tempLCN = StartLCN;
                }
                InUse = BitBuf->Buffer[i] & Mask;
                if (!InUse) {
                    CountEmptyClusters++;
                }
                else {
                    CountEmptyClusters = 0;
                }
                Mask <<= 1;
                StartLCN++;
            }

            if(CountEmptyClusters >= ClusterCount)
            {
                ui->listWidget->addItem("We found enougt clusters! LCN start: " + QString::number(StartLCN));
                ui->listWidget->addItem("Find free cluster count: " + QString::number(CountEmptyClusters));
                StartLCN = tempLCN;
                break;
            }
            Mask = 1;
        }
    }

    CloseHandle(hDevice);
    DrawMap();
    if(OutBuf->ExtentCount > 1)
        ui->defrag_btn->setEnabled(true);
    return true;
}

bool MainWindow::MoveFileClusters()
{
    HANDLE hDrive = 0;
    HANDLE _hFile = 0;

    MOVE_FILE_DATA InBuffer;
    DWORD nBytes = 0;

    std::string disk("\\\\.\\");                 // імя тому на якому розміщено файл
    disk.append(path.toStdString().substr(0,2));

    _hFile = CreateFile((const wchar_t*)path.utf16(), FILE_READ_ATTRIBUTES,FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
    hDrive = CreateFileA(disk.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_EXISTING, 0, 0);

    if(_hFile == INVALID_HANDLE_VALUE || hDrive == INVALID_HANDLE_VALUE)
    {
        ui->listWidget->addItem("invalid handle value!");
        return false;
    }

    InBuffer.FileHandle = _hFile;
    InBuffer.StartingLcn.QuadPart = StartLCN;

    for (unsigned int k = 0; k < OutBuf->ExtentCount; k++)
    {
        InBuffer.StartingVcn.QuadPart = PrevVCN;
        InBuffer.ClusterCount = OutBuf->Extents[k].NextVcn.QuadPart - PrevVCN;
        int ret = DeviceIoControl(hDrive, FSCTL_MOVE_FILE, &InBuffer, sizeof(InBuffer), NULL, 0 , &nBytes, NULL);


        ui->listWidget->addItem("LNC: " + QString::number(InBuffer.StartingLcn.QuadPart));
        ui->listWidget->addItem("Cluster size: " + QString::number(InBuffer.ClusterCount));
        ui->listWidget->addItem("VNC: " + QString::number(InBuffer.StartingVcn.QuadPart));
        ui->listWidget->addItem("MOVE ret code: " + QString::number(ret));
        ret = GetLastError();
        ui->listWidget->addItem("Error code: " + QString::number(ret));

        InBuffer.StartingLcn.QuadPart += InBuffer.ClusterCount;
        PrevVCN = OutBuf->Extents[k].NextVcn.QuadPart;
    }
    ui->listWidget->addItem("Finish!");
    CloseHandle(hDrive);
    CloseHandle(_hFile); //hd
    /**/
    ui->get_map_btn->setEnabled(false);
    ui->getVol_btn->setEnabled(false);

    /**/

    ReDrawMap();
    ui->defrag_btn->setEnabled(false);
    return true;
}

bool MainWindow::OpenExFile(HANDLE &_hFile)
{
    LPCWSTR file_name = (const wchar_t * ) path.utf16();
    _hFile = CreateFile(file_name,FILE_READ_ATTRIBUTES,FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,0);

    if(_hFile == INVALID_HANDLE_VALUE)
    {
        ui->listWidget->addItem("Error! Invalid handle value!");
        return false;
    }
    else
    {
        ui->getVol_btn->setEnabled(true);
        ui->get_map_btn->setEnabled(true);
        return true;
    }
}

void MainWindow::DrawMap()
{
    int ClustersPerSquare = (BitBuf->BitmapSize.QuadPart / 8) / 40000;
    bool flag = false;

    int counter = 0;
    int var = 0;

    std::sort(LCN_FILE.begin(), LCN_FILE.end(),std::greater<int>());

    for(int i = 0; i < 200; i++)
    {
        for(int j = 0; j < 200; j++)
        {
            for(unsigned int k = 0; k < LCN_FILE.size(); k++)
            {
                var = LCN_FILE[k] / 8;

                if( var > counter && var < counter + ClustersPerSquare)
                {
                    //qDebug() << counter;
                    LCN_FILE.pop_back();
                    ui->tableWidget->setItem(i,j, new QTableWidgetItem);
                    ui->tableWidget->item(i,j)->setBackgroundColor(Qt::red);
                    flag = true;
                    break;
                }
            }
            if(flag)
            {
                flag = false;
                counter += ClustersPerSquare;
                continue;
            }

            if(BitBuf->Buffer[counter] > 0)
            {
                ui->tableWidget->setItem(i,j, new QTableWidgetItem);
                ui->tableWidget->item(i,j)->setBackgroundColor(Qt::blue);
            }
            else
            {
                ui->tableWidget->setItem(i,j, new QTableWidgetItem);
                ui->tableWidget->item(i,j)->setBackgroundColor(Qt::white);
            }

            counter += ClustersPerSquare;
        }
    }

    LCN_FILE.clear();
}

void MainWindow::ReDrawMap()
{
    //ui->listWidget->clear();

    if(OutBuf != NULL)
        delete [] OutBuf;

    GetFileMapping();
    GetVolumeBitmap();
    ui->defrag_btn->setEnabled(false);
}

MainWindow::~MainWindow()
{
    if(BitBuf != NULL)
        delete [] BitBuf;
    delete ui;
}

void MainWindow::on_open_btn_clicked()
{
   path = QFileDialog::getOpenFileName(this,"Open","C:\\Users\\Svyatoslav\\Desktop");
   if(!path.isEmpty())
   {
       ui->statusBar->setStatusTip("File " + path + " was opend!");
   }
   else
   {
       ui->statusBar->setStatusTip("File not open!");
       return;
   }

   if(!OpenExFile(hFile))
   {
       ui->listWidget->addItem("File not opend! Error code: " + QString::number(GetLastError()));
       return;
   }
   else
   {
       ui->listWidget->addItem(path + " is opend!");
   }

   if(BitBuf != NULL)
       delete [] BitBuf;
}

void MainWindow::on_get_map_btn_clicked()
{
    GetFileMapping();
}

void MainWindow::on_getVol_btn_clicked()
{
    GetVolumeBitmap();
}

void MainWindow::on_defrag_btn_clicked()
{
    MoveFileClusters();
}

void MainWindow::on_clear_btn_clicked()
{
    ui->listWidget->clear();
}

void MainWindow::on_pushButton_5_clicked()
{
    this->close();
}
