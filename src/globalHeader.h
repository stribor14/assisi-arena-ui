#ifndef GLOBAL
#define GLOBAL

#include <QString>
#include <QDateTime>
#include <QSettings>
#include <QDir>

/*!
 * \brief Global class to store information of loaded project
 *
 * Information stored in this class is loaded from project file (*.assisi) or session file (*.arenaUI).
 * Associated files are loaded automaticaly from source file. If multiple arena layers are detected, user can choose which one to display.
 */
class _assisi{
public :
    /*!
     * \brief Loaded project file (*.assisi)
     */
    QString name;
    /*!
     * \brief Associated CASU configuation file (*.arena)
     */
    QString arenaFile;
    /*!
     * \brief Associated deploymnet file (*.deb)
     */
    QString depFile;
    /*!
     * \brief Associated CASU neighbourhood file (*.nbg)
     */
    QString ngbFile;
    /*!
     * \brief Choosen layer to display in UI
     */
    QString arenaLayer;
};

extern QSettings* settings;
extern QString date_time_format;
extern QString time_format;
extern _assisi assisiFile;

void loadConfig();

#endif // GLOBAL

