#ifndef PDFVIEWERWIDGET_H
#define PDFVIEWERWIDGET_H

#include <QWidget>
#include <QtPdfWidgets/QPdfView>
#include <QtPdf/QPdfDocument>
#include <QtPdf/QPdfPageNavigator>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QDoubleSpinBox>

class QVBoxLayout;

class PdfViewerWidget : public QWidget
{
    Q_OBJECT

public:
    PdfViewerWidget(const QString &filePath, QWidget *parent = nullptr);

private:
    QPdfDocument *doc;
    QPdfView *pdfView;
    QPdfPageNavigator *pageNav;

    QPushButton *prevBtn;
    QPushButton *nextBtn;
    QSpinBox *pageSpin;
    QLabel *pageCountLbl;
    QDoubleSpinBox *zoomSpin;
    QPushButton *fitWidthBtn;

    void setupToolbar(QVBoxLayout *layout);
    void updateNavButtons();
};

#endif
