#include "string_input.hpp"

#include "utility.hpp"

#include <qevent.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qstackedlayout.h>
#include <qtoolbutton.h>

// ----- StringInput ----- //

StringInput::StringInput( Override<QString>& value, const std::function<bool( const QString& )>& validator ) : _value { value }
{
    this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
    this->setStyleSheet( "QLineEdit { border-radius: 3px; background: #FAFAFA; }" );

    _string_input = new QLineEdit {};

    _button_reset = new QToolButton { _string_input };
    _button_reset->setIcon( QIcon { ":/reset.svg" } );
    _button_reset->setCursor( Qt::ArrowCursor );
    _button_reset->setVisible( _value.override_value().has_value() );

    auto layout = new QStackedLayout { this };
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->addWidget( _string_input );

    this->update_string_input();
    QObject::connect( &_value, &Override<double>::value_changed, this, &StringInput::update_string_input );
    QObject::connect( _string_input, &QLineEdit::textEdited, this, [this, validator] ( const QString& string )
    {
        if( validator && !validator( string ) )
        {
            _string_input->setStyleSheet( "QLineEdit { border: 1px solid red; } " );
        }
        else
        {
            _value.update_override_value( string );
        }
    } );
    QObject::connect( _button_reset, &QToolButton::clicked, this, [this]
    {
        _value.update_override_value( std::nullopt );
        _string_input->clearFocus();
    } );

    _string_input->installEventFilter( this );
}

bool StringInput::eventFilter( QObject* object, QEvent* event )
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
void StringInput::update_string_input()
{
    const auto value = _value.value();
    if( !( _string_input->text() == value && _string_input->hasFocus() ) )
    {
        _string_input->setText( value );
        _string_input->setCursorPosition( 0 );
    }

    _string_input->setStyleSheet( "QLineEdit { border: 1px solid #EEEEEE; } " );
    _button_reset->setVisible( _value.override_value().has_value() );
}