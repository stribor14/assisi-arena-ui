#include <boost/lexical_cast.hpp>

#include <QThread>
#include <QGraphicsSceneMouseEvent>
#include <QFileDialog>
#include <QMessageBox>

#include "arenaui.h"
#include "ui_arenaui.h"


using namespace nzmqt;
using namespace std;
using namespace AssisiMsg;
using namespace boost;


// -------------------------------------------------------------------------------

ArenaUI::ArenaUI(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ArenaUI)
{
    loadConfig();
    ui->setupUi(this);
    ui->actionToggleLog->setChecked(settings->value("log_on").toBool());

    //CASU TREE TAB
    ui->casuTree->addAction(ui->actionPlot_selected_in_same_trend);
    ui->casuTree->addAction(ui->actionPlot_selected_in_different_trends);
    ui->casuTree->header()->setSortIndicator(0,Qt::AscendingOrder);

    connect(ui->casuTree,SIGNAL(itemSelectionChanged()),this,SLOT(updateTreeSelection()));

    //TREND TAB SCROLLABLE LAYOUT

    QWidget* tempWidget = new QWidget;
    QScrollArea* tempScroll = new QScrollArea;

    tempScroll->setWidget(tempWidget);
    tempScroll->setWidgetResizable(true);
    trendTab = new QVBoxLayout(tempWidget);

    ui->tab_trend->setLayout(new QVBoxLayout);
    ui->tab_trend->layout()->addWidget(tempScroll);

    //GRAPHICS SCENE
    arena_scene = new QArenaScene(this);
    arena_scene->setSceneRect(0,0,800,800);

    ui->arenaSpace->setScene(arena_scene);
    ui->arenaSpace->setDragMode(QGraphicsView::RubberBandDrag);

    MouseClickHandler* click_handler = new MouseClickHandler(arena_scene, this);
    arena_scene->installEventFilter(click_handler);

    connect(ui->arenaSpace,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(customContextMenu(QPoint)));

    //DEPLOYMENT
    ui->tab_deploy->setLayout(new QVBoxLayout);

    // - deployment header
    tempWidget = new QWidget;
    QGridLayout* tempLayout = new QGridLayout;
    deployWidget = new QDeploy(this);

    deployArena = new QLabel;
    deployNeighborhood = new QLabel;
    deployFile = new QLabel;

    QFont tempFont;
    tempFont.setBold(true);
    QLabel* tempLabel = new QLabel("Arena description file:");
    tempLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    tempLabel->setAlignment(Qt::AlignRight);
    tempLabel->setFont(tempFont);
    tempLayout->addWidget(tempLabel,1,1);
    tempLabel = new QLabel("Neighborhood file:");
    tempLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    tempLabel->setAlignment(Qt::AlignRight);
    tempLabel->setFont(tempFont);
    tempLayout->addWidget(tempLabel,2,1);
    tempLabel = new QLabel("Deployment file:");
    tempLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    tempLabel->setAlignment(Qt::AlignRight);
    tempLabel->setFont(tempFont);
    tempLayout->addWidget(tempLabel,3,1);

    tempLayout->addWidget(deployArena,1,2);
    tempLayout->addWidget(deployNeighborhood,2,2);
    tempLayout->addWidget(deployFile,3,2);

    tempWidget->setLayout(tempLayout);
    ui->tab_deploy->layout()->addWidget(tempWidget);

    // - interaction buttons
    tempWidget = new QWidget;
    tempWidget->setLayout(new FlowLayout);

    QPushButton* tempButton = new QPushButton("Deploy");
    connect(tempButton,SIGNAL(clicked()), deployWidget, SLOT(deploy()));
    tempWidget->layout()->addWidget(tempButton);

    tempButton = new QPushButton("Run");
    connect(tempButton,SIGNAL(clicked()), deployWidget, SLOT(run()));
    tempWidget->layout()->addWidget(tempButton);

    tempButton = new QPushButton("Stop");
    connect(tempButton,SIGNAL(clicked()), deployWidget, SLOT(stop()));
    tempWidget->layout()->addWidget(tempButton);

    tempButton = new QPushButton("Collect data");
    connect(tempButton,SIGNAL(clicked()), deployWidget, SLOT(collect()));
    tempWidget->layout()->addWidget(tempButton);

    tempButton = new QPushButton("Clean log");
    connect(tempButton,SIGNAL(clicked()), deployWidget, SLOT(cleanLog()));
    tempWidget->layout()->addWidget(tempButton);

    tempButton = new QPushButton("Start simulator");
    connect(tempButton,SIGNAL(clicked()), deployWidget, SLOT(simulatorStart()));
    tempWidget->layout()->addWidget(tempButton);

    tempButton = new QPushButton("Stop simulator");
    connect(tempButton,SIGNAL(clicked()), deployWidget, SLOT(simulatorStop()));
    tempWidget->layout()->addWidget(tempButton);

    ui->tab_deploy->layout()->addWidget(tempWidget);

    // - text output window
    deployScroll = new QScrollArea;
    deployScroll->setWidget(deployWidget);
    deployScroll->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    deployScroll->setWidgetResizable(true);
    deployScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    ui->tab_deploy->layout()->addWidget(deployScroll);

    connect(deployScroll->verticalScrollBar(),SIGNAL(rangeChanged(int,int)),this,SLOT(moveDeployScroll(int,int)));

    tempWidget = new QWidget;
    tempWidget->setLayout(new FlowLayout);

    tempButton = new QPushButton("Clear output");
    tempButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(tempButton,SIGNAL(clicked()), deployWidget, SLOT(clear()));
    tempWidget->layout()->addWidget(tempButton);

    QCheckBox* temCheckBox = new QCheckBox("Catch shell output");
    temCheckBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(temCheckBox,SIGNAL(stateChanged(int)), deployWidget, SLOT(toggleOutput(int)));
    tempWidget->layout()->addWidget(temCheckBox);

    ui->tab_deploy->layout()->addWidget(tempWidget);
}

ArenaUI::~ArenaUI()
{
    delete ui;
}

void ArenaUI::sortGraphicsScene()
{
    QList<QGraphicsItem*> tempSelection = arena_scene->selectedItems(); // save active selection
    QPainterPath pp;
    pp.addRect(arena_scene->sceneRect()); // select all -- selectedItems() doesnt return group children which is a case with items()
    arena_scene->setSelectionArea(pp);

    for(int k = 0; k+1 < arena_scene->selectedItems().size(); k++)
        for(int i = k+1; i < arena_scene->selectedItems().size(); i++){
            QGraphicsItem* item1 = arena_scene->selectedItems()[k];
            QGraphicsItem* item2 = arena_scene->selectedItems()[i];
            if(item1->boundingRect().intersects(item2->boundingRect())){
                int z1 = item1->zValue();
                int z2 = item2->zValue();
                if(z1 > z2) swap(z1,z2);

                double area1 = item1->boundingRect().height() * item1->boundingRect().width();
                double area2 = item2->boundingRect().height() * item2->boundingRect().width();

                if(z1 == z2)
                    (area1 > area2 ? item2 : item1)->setZValue(++z1);
                else{
                    (area1 > area2 ? item1 : item2)->setZValue(z1);
                    (area1 > area2 ? item2 : item1)->setZValue(z2);
                }
                //cout << area1 << " " << item1->zValue() << " - " << area2 << " " << item2->zValue() << endl;
            }
        }

    pp = pp.subtracted(pp);
    pp.addRect(0,0,0,0);
    arena_scene->setSelectionArea(pp);
    foreach(QGraphicsItem* item, tempSelection) item->setSelected(true); // after saving items and groups, return selection as was before

}

// -------------------------------------------------------------------------------

MouseClickHandler::MouseClickHandler(QGraphicsScene* scene, QObject *parent) :
    QObject(parent),
    scene_(scene){}

bool MouseClickHandler::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::GraphicsSceneMousePress){
        if(((QGraphicsSceneMouseEvent*)event)->button() == Qt::RightButton) return QObject::eventFilter(obj, event);
        if(QApplication::keyboardModifiers() == Qt::ControlModifier)
            selectedList = scene_->selectedItems();
        else selectedList.clear();

        return true;
    }
    else
    if (event->type() == QEvent::GraphicsSceneMouseMove){
        foreach(QGraphicsItem* item, selectedList) item->setSelected(true);
        drag_true = true;
        scene_->update();

        return true;
    }
    else
    if (event->type() == QEvent::GraphicsSceneMouseRelease)
    {
        foreach(QGraphicsItem* item, selectedList) item->setSelected(true);

        QGraphicsSceneMouseEvent* mouse_click = static_cast<QGraphicsSceneMouseEvent *>(event);
        QGraphicsItem * itemAtMouse= scene_->itemAt(mouse_click->scenePos().x(),mouse_click->scenePos().y(), QTransform());

        if (itemAtMouse && !drag_true)
            if(QApplication::keyboardModifiers() == Qt::ControlModifier){
                if(itemAtMouse->isSelected())itemAtMouse->setSelected(0);
                else itemAtMouse->setSelected(1);
            }
            else{
                scene_->clearSelection();
                itemAtMouse->setSelected(1);
            }
        else if(QApplication::keyboardModifiers() != Qt::ControlModifier && !drag_true)
            scene_->clearSelection();


        drag_true = false;
        scene_->update();

        return true;
    }
    else
    {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
}
//-------------------------------------------------------------------------------
// Subclassed QGraphicsScene for a BUG [QTBUG-10138]
// http://www.qtcentre.org/threads/36953-QGraphicsItem-deselected-on-contextMenuEvent
QArenaScene::QArenaScene(QWidget *parent) : QGraphicsScene(parent){}

void QArenaScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::RightButton) event->accept();
    else QGraphicsScene::mousePressEvent(event);
}
// -------------------------------------------------------------------------------


void ArenaUI::on_actionOpen_Arena_triggered()
{
    QProgressBar progress;
    progress.setMinimum(0);
    QString loadFile = QFileDialog::getOpenFileName(this,tr("Open Arena configuration file"), settings->value("arenaFolder").toString(), tr("All(*.arenaUI *assisi);;Project(*.assisi);;Session(*.arenaUI)"));
    if(!loadFile.size()) return;

    while(trendTab->count()){
        ((QCustomPlot*)trendTab->itemAt(0)->widget())->close();
        trendTab->removeWidget(trendTab->itemAt(0)->widget());
    }

    arena_scene->clear();
    ui->casuTree->clear();



    if(loadFile.endsWith(".assisi")){
        assisiFile.name = loadFile;
        assisiNode = YAML::LoadFile(assisiFile.name.toStdString());

        deployArena->setText(QString::fromStdString(assisiNode["arena"].as<std::string>()));
        deployFile->setText(QString::fromStdString(assisiNode["dep"].as<std::string>()));
        deployNeighborhood->setText(QString::fromStdString(assisiNode["nbg"].as<std::string>()));

        deployWidget->setWorkingDirectory(assisiFile.name.left(assisiFile.name.lastIndexOf('/')));

        assisiFile.arenaFile = assisiFile.name.left(assisiFile.name.lastIndexOf('/')+1) + QString::fromStdString(assisiNode["arena"].as<std::string>());
        assisiFile.depFile = assisiFile.name.left(assisiFile.name.lastIndexOf('/')+1) + QString::fromStdString(assisiNode["dep"].as<std::string>());
        assisiFile.ngbFile = assisiFile.name.left(assisiFile.name.lastIndexOf('/')+1) + QString::fromStdString(assisiNode["nbg"].as<std::string>());

        YAML::Node arenaNode = YAML::LoadFile(assisiFile.arenaFile.toStdString());
        QList<QString> layers;
        for(YAML::const_iterator it=arenaNode.begin(); it!=arenaNode.end(); it++) layers.append(QString::fromStdString(it->first.as<std::string>()));

        if(layers.size() > 1) assisiFile.arenaLayer = QInputDialog::getItem(this,tr("Select arena layer"),"",QStringList(layers));
        else assisiFile.arenaLayer = layers[0];

        progress.setMaximum(arenaNode[assisiFile.arenaLayer.toStdString()].size());
        progress.show();
        progress.move(ui->arenaSpace->mapToGlobal(QPoint(400-progress.width()/2,400-progress.height()/2)));

        for(YAML::const_iterator it=arenaNode[assisiFile.arenaLayer.toStdString()].begin(); it!=arenaNode[assisiFile.arenaLayer.toStdString()].end(); it++){
            QString name = QString::fromStdString(it->first.as<std::string>());
            int xpos = arenaNode[assisiFile.arenaLayer.toStdString()][name.toStdString()]["pose"]["x"].as<int>();
            int ypos = arenaNode[assisiFile.arenaLayer.toStdString()][name.toStdString()]["pose"]["y"].as<int>();
            int yaw = arenaNode[assisiFile.arenaLayer.toStdString()][name.toStdString()]["pose"]["yaw"].as<int>();

            QCasuTreeItem* tempTree = new QCasuTreeItem(ui->casuTree, name);

            ui->casuTree->addTopLevelItem(tempTree);

            QCasuSceneItem* tempItem = new QCasuSceneItem(this, xpos*10+400, -ypos*10+400, yaw, tempTree);

            arena_scene->addItem(tempItem);

            tempTree->setAddr(QString::fromStdString(arenaNode[assisiFile.arenaLayer.toStdString()][name.toStdString()]["sub_addr"].as<std::string>()),
                               QString::fromStdString(arenaNode[assisiFile.arenaLayer.toStdString()][name.toStdString()]["pub_addr"].as<std::string>()),
                               QString::fromStdString(arenaNode[assisiFile.arenaLayer.toStdString()][name.toStdString()]["msg_addr"].as<std::string>()));

            progress.setValue(progress.value()+1);
            QApplication::processEvents();
        }
    }
    else if(loadFile.endsWith(".arenaUI")){

        QSettings loadSession(loadFile,QSettings::IniFormat);

        //GENERAL INFORMATION
        assisiFile.name = loadSession.value("assisiFile").toString();
        assisiFile.arenaLayer = loadSession.value("arenaLayer").toString();

        assisiNode = YAML::LoadFile(assisiFile.name.toStdString());

        deployArena->setText(QString::fromStdString(assisiNode["arena"].as<std::string>()));
        deployFile->setText(QString::fromStdString(assisiNode["dep"].as<std::string>()));
        deployNeighborhood->setText(QString::fromStdString(assisiNode["nbg"].as<std::string>()));

        deployWidget->setWorkingDirectory(assisiFile.name.left(assisiFile.name.lastIndexOf('/')));

        assisiFile.arenaFile = assisiFile.name.left(assisiFile.name.lastIndexOf('/')+1) + QString::fromStdString(assisiNode["arena"].as<std::string>());
        assisiFile.depFile = assisiFile.name.left(assisiFile.name.lastIndexOf('/')+1) + QString::fromStdString(assisiNode["dep"].as<std::string>());
        assisiFile.ngbFile = assisiFile.name.left(assisiFile.name.lastIndexOf('/')+1) + QString::fromStdString(assisiNode["nbg"].as<std::string>());

        YAML::Node arenaNode = YAML::LoadFile(assisiFile.arenaFile.toStdString());
        QMap<QString, QCasuTreeItem*> linker;

        //LOAD AND SPAWN CASUs
        loadSession.beginGroup("sceneHierarchy");
        int tempSize = loadSession.beginReadArray("main");
        progress.setMaximum(progress.maximum() + tempSize);
        groupLoad(&arenaNode, &loadSession, tempSize, &linker, &progress);
        loadSession.endArray();
        loadSession.endGroup();
        this->sortGraphicsScene();

        //LOAD AND SET CONNECTIONS
        loadSession.beginGroup("connectionSettings");
        tempSize = loadSession.beginReadArray("addresses");
        for(int k = 0; k < tempSize; k++){
            loadSession.setArrayIndex(k);
            linker[loadSession.value("casuName").toString()]->setAddr(loadSession.value("sub_addr").toString(),
                                                                      loadSession.value("pub_addr").toString(),
                                                                      loadSession.value("msg_addr").toString());
        }
        loadSession.endArray();
        loadSession.endGroup();

        //LOAD AND LINK TREND GRAPHS
        loadSession.beginGroup("trendGraphs");
        tempSize = loadSession.beginReadArray("plot");
        for(int k = 0; k < tempSize; k++){
            loadSession.setArrayIndex(k);
            int graphSize = loadSession.beginReadArray("graph");
            QList<QTreeWidgetItem*> toAdd;

            for(int i = 0; i < graphSize; i++){
                loadSession.setArrayIndex(i);
                QString graphName = loadSession.value("data").toString();
                QString casuName = graphName.left(graphName.indexOf(":"));

                QSet<QTreeWidgetItem*> temp = linker[casuName]->widget_IR_children.toSet() +
                        linker[casuName]->widget_temp_children.toSet();

                foreach(QTreeWidgetItem* item, temp)
                    if(((QTreeBuffer*)item)->legendName == graphName)
                        toAdd.append(item);
            }
            if(toAdd.size()){
                QTrendPlot* tempWidget = new QTrendPlot(ui->casuTree);
                trendTab->addWidget(tempWidget);
                tempWidget->addGraphList(toAdd);
                tempWidget->setWindowTitle(assisiFile.arenaLayer);
            }

            loadSession.endArray();
        }
        loadSession.endArray();
        loadSession.endGroup();
    }

    arena_scene->addItem(new QColorbar());
}

void ArenaUI::on_actionGroup_triggered()
{
    QList<QGraphicsItem *> temp_itemList= arena_scene->selectedItems();
    arena_scene->clearSelection();
    QGraphicsItemGroup *tempGroup = arena_scene->createItemGroup(temp_itemList);
    tempGroup->setFlag(QGraphicsItem::ItemIsSelectable, true);
    tempGroup->setSelected(1);
    this->sortGraphicsScene();
}


void ArenaUI::on_actionUngroup_triggered()
{
    QList<QGraphicsItem *> temp_itemList= arena_scene->selectedItems();
    foreach(QGraphicsItem* item, temp_itemList)
        if(item->childItems().size()){
            QList<QGraphicsItem *> tempChildList = item->childItems();
            if(!item->childItems().size())item->setZValue(1);
            arena_scene->destroyItemGroup((QGraphicsItemGroup*)item);
            for(int i=0;i<tempChildList.size();i++){;
                tempChildList[i]->setSelected(0);
                tempChildList[i]->setSelected(1);
            }
        }
    this->sortGraphicsScene();
}

void ArenaUI::on_actionConnect_triggered()
{
    bool error = false;
    if (arena_scene->selectedItems().size() != 1) error = true; //Check if excactly one object is selected
    if (arena_scene->selectedItems().size() == 1)
        if(arena_scene->selectedItems()[0]->childItems().size()) error = true; // Check if object is single casu (no children)
    if(error){
        QMessageBox msgBox;
        msgBox.setWindowTitle("ERROR");
        msgBox.setText("Please select one CASU.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        return;
    }
    QCasuSceneItem* temp = (QCasuSceneItem*)arena_scene->selectedItems()[0];
    QDialogConnect* addrDiag = new QDialogConnect(temp->treeItem->sub_addr,temp->treeItem->pub_addr,temp->treeItem->msg_addr);
    if(addrDiag->exec()){
        temp->treeItem->setAddr(addrDiag->sub_addr->text(), addrDiag->pub_addr->text(),addrDiag->msg_addr->text());
    }
}

void ArenaUI::on_actionToggleLog_triggered()
{
    ui->actionToggleLog->setChecked(settings->value("log_on").toBool());
    QString question = QString("Are you sure you want to turn ") + (settings->value("log_on").toBool() ? QString("OFF") : QString("ON")) + QString(" logging?");
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Toggle Log", question , QMessageBox::Yes|QMessageBox::No);
    if(reply == QMessageBox::Yes)settings->setValue("log_on", !settings->value("log_on").toBool());
    ui->actionToggleLog->setChecked(settings->value("log_on").toBool());
}

void ArenaUI::on_actionPlot_selected_in_same_trend_triggered()
{
    QTrendPlot* tempWidget = new QTrendPlot(ui->casuTree);
    trendTab->addWidget(tempWidget);

    tempWidget->addSelectedGraphs();
    tempWidget->setWindowTitle(assisiFile.arenaLayer);
}

void ArenaUI::on_actionPlot_selected_in_different_trends_triggered()
{
    foreach (QTreeWidgetItem* item, ui->casuTree->selectedItems()) {
        QList<QTreeWidgetItem*> tempList;
        tempList.append(item);

        QTrendPlot* tempWidget = new QTrendPlot(ui->casuTree);
        trendTab->addWidget(tempWidget);

        tempWidget->addGraphList(tempList);
        tempWidget->setWindowTitle(assisiFile.arenaLayer);
    }
}

void ArenaUI::on_actionSettings_triggered()
{
    QDialogSettings* settingsDiag = new QDialogSettings();
    settingsDiag->exec();
}

void ArenaUI::toggleIR()
{
    settings->setValue("IR_on",!settings->value("IR_on").toBool());
}

void ArenaUI::toggleTemp()
{
    settings->setValue("temp_on",!settings->value("temp_on").toBool());
}

void ArenaUI::toggleAir()
{
    settings->setValue("air_on",!settings->value("air_on").toBool());
}

void ArenaUI::updateTreeSelection()
{
    if(ui->casuTree->selectedItems().size()){
        ui->actionPlot_selected_in_same_trend->setEnabled(true);
        ui->actionPlot_selected_in_different_trends->setEnabled(true);
    }
    else{
        ui->actionPlot_selected_in_same_trend->setEnabled(false);
        ui->actionPlot_selected_in_different_trends->setEnabled(false);
    }
}

void ArenaUI::customContextMenu(QPoint pos)
{
    QMenu* menu = new QMenu(); //no parent because it inherits background image from QGraphicsView
    QAction* temp;

    menu->setAttribute(Qt::WA_DeleteOnClose);

    bool error_single = false;
    if (arena_scene->selectedItems().size() != 1) error_single = true; //Check if excactly one object is selected
    if (arena_scene->selectedItems().size() == 1)
        if(arena_scene->selectedItems()[0]->childItems().size()) error_single = true; // Check if object is single casu (no children)

    bool error_selected = !arena_scene->selectedItems().size();

    menu->addAction(settings->value("IR_on").toBool() ? "Hide proximity sensors" : "Show proximity sensors",this,SLOT(toggleIR()));
    menu->addAction(settings->value("temp_on").toBool() ? "Hide temperature sensors" : "Show temperature sensors",this,SLOT(toggleTemp()));
    menu->addAction(settings->value("air_on").toBool() ? "Hide airflow marker" : "Show airflow marker",this,SLOT(toggleTemp()));
    menu->addSeparator();
    temp = menu->addAction("Group selected",this,SLOT(on_actionGroup_triggered()));
    if(error_selected) temp->setEnabled(false);
    temp = menu->addAction("Ungroup selected",this,SLOT(on_actionUngroup_triggered()));
    if(error_selected) temp->setEnabled(false);
    menu->addSeparator();
    temp = menu->addAction("Set connection",this,SLOT(on_actionConnect_triggered()));
    if(error_single) temp->setEnabled(false);

    QMenu* sendMenu = new QMenu("Setpoint");
    if(error_selected) sendMenu->setEnabled(false);
    menu->addMenu(sendMenu);

    QSignalMapper* signalMapper = new QSignalMapper(menu);

    temp = sendMenu->addAction("Temperature",signalMapper,SLOT(map()));
    signalMapper->setMapping(temp,"Temperature");
    temp = sendMenu->addAction("Vibration",signalMapper,SLOT(map()));
    signalMapper->setMapping(temp,"Vibration");
    temp = sendMenu->addAction("Airflow",signalMapper,SLOT(map()));
    signalMapper->setMapping(temp,"Airflow");
    temp = sendMenu->addAction("LED",signalMapper,SLOT(map()));
    signalMapper->setMapping(temp,"LED");

    connect(signalMapper,SIGNAL(mapped(QString)),this,SLOT(sendSetpoint(QString)));

    menu->popup(ui->arenaSpace->mapToGlobal(pos));
}

void ArenaUI::moveDeployScroll(int min, int max)
{
    Q_UNUSED(min)
    deployScroll->verticalScrollBar()->setValue(max);
}

void ArenaUI::sendSetpoint(QString actuator)
{
    QDialogSetpoint* dialog = new QDialogSetpoint(actuator);
    if(dialog->exec())
        groupSendSetpoint(arena_scene->selectedItems(),dialog->message);
}

void ArenaUI::groupSendSetpoint(QList<QGraphicsItem *> group, QList<QByteArray> message)
{
    foreach(QGraphicsItem* item, group)
        if(item->childItems().size())
            groupSendSetpoint(item->childItems(),message);
        else
            ((QCasuSceneItem*)item)->treeItem->sendSetpoint(message);
}

void ArenaUI::groupSave(QSettings *saveState, QList<QGraphicsItem *> group, QString groupName)
{
    saveState->beginWriteArray(groupName);
    for(int k=0; k < group.size() ; k++){
        saveState->setArrayIndex(k);
        if(!group[k]->childItems().size()) saveState->setValue("casuName",((QCasuSceneItem*)group[k])->treeItem->casuName);
        else groupSave(saveState, group[k]->childItems(), "group");
    }
    saveState->endArray();
}

QList<QGraphicsItem *>* ArenaUI::groupLoad(YAML::Node* arenaNode, QSettings *loadState, int groupSize, QMap<QString, QCasuTreeItem*>* linker, QProgressBar* progress)
{
    QList<QGraphicsItem *>* returnGroup = new QList<QGraphicsItem *>;
    for(int k=0; k < groupSize; k++){
        loadState->setArrayIndex(k);
        progress->setValue(progress->value()+1);
        QGraphicsItem* tempItem;

        if(loadState->childKeys().size()){
            QString name = loadState->value("casuName").toString();
            int xpos = (*arenaNode)[assisiFile.arenaLayer.toStdString()][name.toStdString()]["pose"]["x"].as<int>();
            int ypos =(*arenaNode)[assisiFile.arenaLayer.toStdString()][name.toStdString()]["pose"]["y"].as<int>();
            int yaw = (*arenaNode)[assisiFile.arenaLayer.toStdString()][name.toStdString()]["pose"]["yaw"].as<int>();

            QCasuTreeItem* tempTree = new QCasuTreeItem(ui->casuTree, name);

            ui->casuTree->addTopLevelItem(tempTree);

            tempItem = new QCasuSceneItem(this, xpos*10+400, -ypos*10+400, yaw, tempTree);

            arena_scene->addItem(tempItem);

            linker->insert(name, tempTree);

            progress->setValue(progress->value()+1);
        }
        else{
            int tempSize = loadState->beginReadArray("group");
            progress->setMaximum(progress->maximum() + tempSize);
            tempItem = (QGraphicsItem*) arena_scene->createItemGroup(*groupLoad(arenaNode, loadState, tempSize, linker, progress));
            tempItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
            tempItem->setSelected(0);
            tempItem->setZValue(-1);
            loadState->endArray();
        }
        returnGroup->append(tempItem);
        QApplication::processEvents();
    }
   return returnGroup;
}

void ArenaUI::on_actionSave_triggered()
{
    QString saveFile = QFileDialog::getSaveFileName(this,tr("Save As"),settings->value("arenaFolder").toString(),tr("*.arenaUI"));

    if(!saveFile.size())return;
    if(!saveFile.endsWith(".arenaUI")) saveFile += ".arenaUI";
    if(QFile(saveFile).exists())QFile(saveFile).remove();

    QSettings saveState(saveFile,QSettings::IniFormat);

    saveState.setValue("assisiFile",assisiFile.name);
    saveState.setValue("arenaLayer",assisiFile.arenaLayer);

    //save CASU graphics scene

    QList<QGraphicsItem*> tempSelection = arena_scene->selectedItems(); // save active selection
    QPainterPath pp;
    pp.addRect(arena_scene->sceneRect()); // select all -- selectedItems() doesnt return group children which is a case with items()
    arena_scene->setSelectionArea(pp);

    saveState.beginGroup("sceneHierarchy");
    groupSave(&saveState, arena_scene->selectedItems(),"main");
    saveState.endGroup();

    pp = pp.subtracted(pp);
    pp.addRect(0,0,0,0);
    arena_scene->setSelectionArea(pp);
    foreach(QGraphicsItem* item, tempSelection) item->setSelected(true); // after saving items and groups, return selection as was before

    //save trend position and graphs

    saveState.beginGroup("trendGraphs");
    saveState.beginWriteArray("plot");
    for(int k=0; k < trendTab->count() ; k++){
        QTrendPlot* tempPlot = (QTrendPlot*) trendTab->itemAt(k)->widget();
        saveState.setArrayIndex(k);
        saveState.beginWriteArray("graph");
        for(int i=0; i < tempPlot->graphCount(); i++){
            saveState.setArrayIndex(i);
            saveState.setValue("data",tempPlot->connectionMap[tempPlot->graph(i)]->legendName);
        }
        saveState.endArray();
    }
    saveState.endArray();
    saveState.endGroup();

    //save connection settins

    saveState.beginGroup("connectionSettings");
    saveState.beginWriteArray("addresses");
    for(int k=0; k < ui->casuTree->topLevelItemCount(); k++){
        saveState.setArrayIndex(k);
        saveState.setValue("casuName",((QCasuTreeItem*)ui->casuTree->topLevelItem(k))->casuName);
        saveState.setValue("sub_addr",((QCasuTreeItem*)ui->casuTree->topLevelItem(k))->sub_addr);
        saveState.setValue("pub_addr",((QCasuTreeItem*)ui->casuTree->topLevelItem(k))->pub_addr);
        saveState.setValue("msg_addr",((QCasuTreeItem*)ui->casuTree->topLevelItem(k))->msg_addr);
    }
    saveState.endArray();
    saveState.endGroup();
}