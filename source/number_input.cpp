#include "number_input.hpp"

#include "logger.hpp"
#include "utility.hpp"

#include <qevent.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qstackedlayout.h>
#include <qtoolbutton.h>
#include <qvalidator.h>

// ----- NumberInput ----- //

NumberInput::NumberInput( Override<double>& value, const std::function<bool( double )>& validator ) : _value { value }
{
    this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
    this->setStyleSheet( "QLineEdit { border-radius: 3px; background: #FAFAFA; }" );

    _string_input = new QLineEdit {};
    _string_input->setValidator( new QDoubleValidator );

    _button_reset = new QToolButton { _string_input };
    _button_reset->setIcon( QIcon { ":/reset.svg" } );
    _button_reset->setCursor( Qt::ArrowCursor );
    _button_reset->setVisible( _value.override_value().has_value() );

    auto layout = new QStackedLayout { this };
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->addWidget( _string_input );

    this->update_string_input();
    QObject::connect( &_value, &Override<double>::value_changed, this, &NumberInput::update_string_input );
    QObject::connect( _string_input, &QLineEdit::textEdited, this, [this, validator] ( const QString& string )
    {
        const auto value = string.toDouble();
        if( validator && !validator( value ) )
        {
            _string_input->setStyleSheet( "QLineEdit { border: 1px solid red; } " );
        }
        else
        {
            _value.update_override_value( value );
        }
    } );
    QObject::connect( _button_reset, &QToolButton::clicked, this, [this]
    {
        _value.update_override_value( std::nullopt );
        _string_input->clearFocus();
    } );

    _string_input->installEventFilter( this );
}

bool NumberInput::eventFilter( QObject* object, QEvent* event )
{
    if( object == _string_input )
    {
        if( event->type() == QEvent::Resize )
        {
            _button_reset->resize( _string_input->height() - 2, _string_input->height() - 2 );
            _button_reset->move( _string_input->rect().right() - _button_reset->width(), ( _string_input->height() - _button_reset->height() ) / 2 );
        }
        else if( event->type() == QEvent::FocusOut )
        {
            this->update_string_input();
        }
    }

    return QObject::eventFilter( object, event );
}
void NumberInput::update_string_input()
{
    const auto value = _value.value();
    if( !( _string_input->text().toDouble() == value && _string_input->hasFocus() ) )
    {
        _string_input->setText( QString::number( value, 'f', utility::compute_precision( value ) ) );
        _string_input->setCursorPosition( 0 );
    }

    _string_input->setStyleSheet( "QLineEdit { border: 1px solid #EEEEEE; } " );
    _button_reset->setVisible( _value.override_value().has_value() );
}

// ----- RangeInput ----- //

RangeInput::RangeInput( Override<double>& lower, Override<double>& upper )
{
    this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );

    auto layout = new QHBoxLayout { this };
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( 3 );

    auto lower_input = new NumberInput { lower, [&upper] ( double value ) { return value < upper.value(); } };
    auto upper_input = new NumberInput { upper, [&lower] ( double value ) { return value > lower.value(); } };

    layout->addWidget( lower_input );
    layout->addWidget( new QLabel { " \u2014 " } );
    layout->addWidget( upper_input );
}