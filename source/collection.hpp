#pragma once
#include <qobject.h>
#include <qsharedpointer.h>

class CollectionObject : public QObject
{
    Q_OBJECT
signals:
    void object_count_changed( qsizetype object_count ) const;
    void object_appended( QSharedPointer<QObject> object ) const;
    void object_removed( QSharedPointer<QObject> object ) const;
};

template<class T> class Collection : public CollectionObject
{
public:
    using value_type = T;

    virtual qsizetype object_count() const noexcept = 0;
    virtual QSharedPointer<value_type> object( qsizetype index ) const = 0;

    virtual void append( QSharedPointer<value_type> object ) = 0;
    virtual void remove( QSharedPointer<value_type> object ) = 0;
    void clear()
    {
        while( this->object_count() )
        {
            this->remove( this->object( 0 ) );
        }
    }

    const QSharedPointer<value_type> first() const
    {
        return this->object( 0 );
    }
    QSharedPointer<value_type> first()
    {
        return this->object( 0 );
    }

    const QSharedPointer<value_type> last() const
    {
        return this->object( this->object_count() - 1 );
    }
    QSharedPointer<value_type> last()
    {
        return this->object( this->object_count() - 1 );
    }

    template<class U> class type_filter;
};

template<class T> class Storage : public Collection<T>
{
public:
    using value_type = T;

    qsizetype object_count() const noexcept
    {
        return _objects.size();
    }
    QSharedPointer<value_type> object( qsizetype index ) const
    {
        return _objects.at( index );
    }

    void append( QSharedPointer<value_type> object ) override
    {
        _objects.append( object );
        emit CollectionObject::object_appended( object );
        emit CollectionObject::object_count_changed( this->object_count() );
    }
    void remove( QSharedPointer<value_type> object ) override
    {
        if( _objects.removeAll( object ) )
        {
            emit CollectionObject::object_removed( object );
            emit CollectionObject::object_count_changed( this->object_count() );
        }
    }

    const auto begin() const
    {
        return _objects.begin();
    }
    const auto end() const
    {
        return _objects.end();
    }

    auto begin()
    {
        return _objects.begin();
    }
    auto end()
    {
        return _objects.end();
    }

private:
    QList<QSharedPointer<value_type>> _objects;
};

template<class T> template <class U> class Collection<T>::type_filter : public Collection<U>
{
public:
    using value_type = U;

    type_filter( QSharedPointer<Collection<T>> collection ) : _collection { collection }
    {
        if( auto collection_pointer = _collection.lock() )
        {
            QObject::connect( collection_pointer.data(), &CollectionObject::object_appended, this, [this] ( QSharedPointer<QObject> pointer )
            {
                if( auto object = pointer.objectCast<value_type>(); object )
                {
                    _objects.append( object );
                    emit CollectionObject::object_appended( object );
                    emit CollectionObject::object_count_changed( this->object_count() );
                }
            } );
            QObject::connect( collection_pointer.data(), &CollectionObject::object_removed, this, [this] ( QSharedPointer<QObject> pointer )
            {
                if( auto object = pointer.objectCast<value_type>(); object )
                {
                    if( _objects.removeAll( object ) )
                    {
                        emit CollectionObject::object_removed( object );
                        emit CollectionObject::object_count_changed( this->object_count() );
                    }
                }
            } );
            QObject::connect( collection_pointer.data(), &CollectionObject::destroyed, this, [this]
            {
                this->clear();
            } );

            for( qsizetype i = 0; i < collection_pointer->object_count(); ++i )
            {
                if( auto object = collection_pointer->object( i ).objectCast<value_type>(); object )
                {
                    _objects.append( object );
                }
            }
        }
    }

    qsizetype object_count() const noexcept
    {
        return _objects.size();
    }
    QSharedPointer<value_type> object( qsizetype index ) const
    {
        return _objects.at( index ).lock();
    }

    void append( QSharedPointer<value_type> object ) override
    {
        _collection.lock()->append( object );
    }
    void remove( QSharedPointer<value_type> object ) override
    {
        _collection.lock()->remove( object );
    }

    const auto begin() const
    {
        return _objects.begin();
    }
    const auto end() const
    {
        return _objects.end();
    }

    auto begin()
    {
        return _objects.begin();
    }
    auto end()
    {
        return _objects.end();
    }

private:
    QWeakPointer<Collection<T>> _collection;
    QList<QWeakPointer<value_type>> _objects;
};