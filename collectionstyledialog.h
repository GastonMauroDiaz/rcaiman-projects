#ifndef COLLECTIONSTYLEDIALOG_H
#define COLLECTIONSTYLEDIALOG_H

#include "collectionstylefile.h"
#include "project.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;
class QStackedWidget;

class CollectionStyleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CollectionStyleDialog(const Collection &collection,
                                   QWidget *parent = nullptr);

private:
    QWidget *createPointEditor();
    QWidget *createImageEditor();
    CollectionStyleData pointData() const;
    CollectionStyleData imageData() const;
    void loadInitialStyle();
    void saveStyle();
    void resetStyle();
    void choosePointColor();
    void updateColorButton();
    void updateImageVisibility();
    QStringList pointLabelFields() const;

    Collection m_collection;
    CollectionStyleData m_initialData;

    QPushButton *m_colorButton = nullptr;
    QDoubleSpinBox *m_pointSize = nullptr;
    QComboBox *m_symbol = nullptr;
    QComboBox *m_labelField = nullptr;
    QDoubleSpinBox *m_labelSize = nullptr;

    QDoubleSpinBox *m_opacity = nullptr;
    QComboBox *m_mode = nullptr;
    QComboBox *m_ramp = nullptr;
    QComboBox *m_stretch = nullptr;
    QDoubleSpinBox *m_gamma = nullptr;
    QSpinBox *m_band = nullptr;
    QSpinBox *m_redBand = nullptr;
    QSpinBox *m_greenBand = nullptr;
    QSpinBox *m_blueBand = nullptr;
    QCheckBox *m_tailTrim = nullptr;
    QDoubleSpinBox *m_trim = nullptr;
};

#endif // COLLECTIONSTYLEDIALOG_H
