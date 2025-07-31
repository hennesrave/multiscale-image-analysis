#pragma once
#include "utility.hpp"

#include <qwidget.h>

class QLineEdit;
class QToolButton;

// ----- StringInput ----- //

class StringInput : public QWidget
{
	Q_OBJECT
public:
	StringInput( Override<QString>& value, const std::function<bool( const QString& )>& validator = {} );

private:
	bool eventFilter( QObject* object, QEvent* event ) override;
	void update_string_input();

	Override<QString>& _value;
	QLineEdit* _string_input = nullptr;
	QToolButton* _button_reset = nullptr;
	Override<int> _precision { 2, std::nullopt };
};