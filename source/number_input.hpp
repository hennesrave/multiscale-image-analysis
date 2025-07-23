#pragma once
#include "utility.hpp"

#include <qwidget.h>

class QLineEdit;
class QToolButton;

// ----- NumberInput ----- //

class NumberInput : public QWidget
{
    Q_OBJECT
public:
    NumberInput( Override<double>& value, const std::function<bool( double )>& validator = {} );

private:
    bool eventFilter( QObject* object, QEvent* event ) override;
    void update_string_input();

    Override<double>& _value;
    QLineEdit* _string_input = nullptr;
    QToolButton* _button_reset = nullptr;
};

// ----- RangeInput ----- //

class RangeInput : public QWidget
{
    Q_OBJECT
public:
    RangeInput( Override<double>& lower, Override<double>& upper );
};