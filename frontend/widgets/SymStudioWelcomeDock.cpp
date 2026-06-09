#include "SymStudioWelcomeDock.hpp"

#include <QLabel>
#include <QVBoxLayout>

SymStudioWelcomeDock::SymStudioWelcomeDock(OBSBasic *main_, QWidget *parent) : QWidget(parent), main(main_)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(12, 12, 12, 12);
	layout->setSpacing(10);

	QLabel *title = new QLabel(QStringLiteral("Welcome to SymStudio"), this);
	title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #00E5FF;"));
	layout->addWidget(title);

	layout->addStretch(1);
	setLayout(layout);
}
