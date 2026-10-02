#include "pdfviewerwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>

PdfViewerWidget::PdfViewerWidget(const QString &filePath, QWidget *parent)
    : QWidget(parent)
{
    doc = new QPdfDocument(this);
    doc->load(filePath);

    pdfView = new QPdfView();
    pdfView->setDocument(doc);
    pdfView->setPageMode(QPdfView::PageMode::MultiPage);
    pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    pdfView->setStyleSheet("background-color: #525659;");

    pageNav = pdfView->pageNavigator();

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setupToolbar(mainLayout);
    mainLayout->addWidget(pdfView, 1);

    connect(pageNav, &QPdfPageNavigator::currentPageChanged, this, [this](int page) {
        pageSpin->setValue(page + 1);
        updateNavButtons();
    });

    connect(doc, &QPdfDocument::pageCountChanged, this, [this](int count) {
        pageCountLbl->setText("de " + QString::number(count));
        pageSpin->setMaximum(count);
        updateNavButtons();
    });
}

void PdfViewerWidget::setupToolbar(QVBoxLayout *layout)
{
    QWidget *bar = new QWidget();
    bar->setFixedHeight(40);
    bar->setStyleSheet(
        "QWidget { background-color: #2d3842; }"
        "QPushButton { background-color: transparent; color: #dcdcdc; border: none;"
        "  padding: 4px 10px; border-radius: 4px; font-size: 13px; }"
        "QPushButton:hover { background-color: #3a4955; }"
        "QPushButton:disabled { color: #5a6b7a; }"
        "QLabel { color: #dcdcdc; font-size: 13px; background: transparent; }"
        "QSpinBox { background-color: #19232d; color: #dcdcdc; border: 1px solid #5a6b7a;"
        "  border-radius: 4px; padding: 2px 6px; min-width: 50px; }"
        "QDoubleSpinBox { background-color: #19232d; color: #dcdcdc; border: 1px solid #5a6b7a;"
        "  border-radius: 4px; padding: 2px 6px; min-width: 60px; }"
    );

    QHBoxLayout *h = new QHBoxLayout(bar);
    h->setContentsMargins(8, 0, 8, 0);
    h->setSpacing(4);

    prevBtn = new QPushButton(QStringLiteral("\u25C0"));
    prevBtn->setToolTip("P\u00E1gina anterior");
    connect(prevBtn, &QPushButton::clicked, this, [this]() {
        int pg = pageNav->currentPage();
        if (pg > 0) pageNav->jump(pg - 1, QPointF(), 0);
    });
    h->addWidget(prevBtn);

    nextBtn = new QPushButton(QStringLiteral("\u25B6"));
    nextBtn->setToolTip("P\u00E1gina siguiente");
    connect(nextBtn, &QPushButton::clicked, this, [this]() {
        int pg = pageNav->currentPage();
        if (pg < doc->pageCount() - 1) pageNav->jump(pg + 1, QPointF(), 0);
    });
    h->addWidget(nextBtn);

    h->addSpacing(8);

    pageSpin = new QSpinBox();
    pageSpin->setMinimum(1);
    pageSpin->setMaximum(1);
    pageSpin->setValue(1);
    connect(pageSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int val) {
        pageNav->jump(val - 1, QPointF(), 0);
    });
    h->addWidget(pageSpin);

    pageCountLbl = new QLabel("de 1");
    h->addWidget(pageCountLbl);

    h->addStretch();

    zoomSpin = new QDoubleSpinBox();
    zoomSpin->setMinimum(25);
    zoomSpin->setMaximum(400);
    zoomSpin->setValue(100);
    zoomSpin->setSuffix("%");
    zoomSpin->setSingleStep(10);
    connect(zoomSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
        pdfView->setZoomFactor(val / 100.0);
    });
    h->addWidget(zoomSpin);

    fitWidthBtn = new QPushButton("Ajustar");
    fitWidthBtn->setToolTip("Ajustar al ancho");
    connect(fitWidthBtn, &QPushButton::clicked, this, [this]() {
        pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        zoomSpin->setValue(qRound(pdfView->zoomFactor() * 100));
    });
    h->addWidget(fitWidthBtn);

    layout->addWidget(bar);
}

void PdfViewerWidget::updateNavButtons()
{
    prevBtn->setEnabled(pageNav->currentPage() > 0);
    nextBtn->setEnabled(pageNav->currentPage() < doc->pageCount() - 1);
}
