#pragma once

#include <cstdint>
#include <atomic>

namespace gamescope
{
    template <bool bReferenceOwns = true>
    class RcObject
    {
    public:
        static constexpr bool ReferenceOwnsObject()
        {
            return bReferenceOwns;
        }

        inline uint32_t IncRef()
        {
            return ++m_uRefCount;
        }

        inline uint32_t DecRef()
        {
            return --m_uRefCount;
        }

        inline uint32_t GetRefCount()
        {
            return m_uRefCount;
        }

    private:
        std::atomic<uint32_t> m_uRefCount{ 0u };
    };

    template <bool bReferenceOwns = true>
    class IRcObject : public RcObject<bReferenceOwns>
    {
    public:
        virtual ~IRcObject()
        {
        }

        virtual uint32_t IncRef()
        {
            return RcObject<bReferenceOwns>::IncRef();
        }

        virtual uint32_t DecRef()
        {
            return RcObject<bReferenceOwns>::DecRef();
        }
    };

    template <typename T>
    class Rc
    {
        template <typename Tx>
        friend class Rc;

    public:
        Rc() { }
        Rc( std::nullptr_t ) { }

        Rc( T* pObject )
        : m_pObject{ pObject }
        {
            this->IncRef();
        }

        Rc( const Rc& other )
            : m_pObject{ other.m_pObject }
        {
            this->IncRef();
        }

        template <typename Tx>
        Rc( const Rc<Tx>& other )
            : m_pObject{ other.m_pObject }
        {
            this->IncRef();
        }

        Rc( Rc&& other )
            : m_pObject{ other.m_pObject }
        {
            other.m_pObject = nullptr;
        }

        template <typename Tx>
        Rc( Rc<Tx>&& other )
            : m_pObject{ other.m_pObject }
        {
            other.m_pObject = nullptr;
        }

        Rc& operator = ( std::nullptr_t )
        {
            this->DecRef();
            m_pObject = nullptr;
            return *this;
        }

        Rc& operator = ( const Rc& other )
        {
            other.IncRef();
            this->DecRef();
            m_pObject = other.m_pObject;
            return *this;
        }

        template <typename Tx>
        Rc& operator = ( const Rc<Tx>& other )
        {
            other.IncRef();
            this->DecRef();
            m_pObject = other.m_pObject;
            return *this;
        }

        Rc& operator = ( Rc&& other )
        {
            this->DecRef();
            this->m_pObject = other.m_pObject;
            other.m_pObject = nullptr;
            return *this;
        }

        template <typename Tx>
        Rc& operator = ( Rc<Tx>&& other )
        {
            this->DecRef();
            this->m_pObject = other.m_pObject;
            other.m_pObject = nullptr;
            return *this;
        }

        ~Rc()
        {
            this->DecRef();
        }

        T& operator *  () const { return *m_pObject; }
        T* operator -> () const { return  m_pObject; }
        T* get() const { return m_pObject; }

        bool operator == ( const Rc& other ) const { return m_pObject == other.m_pObject; }
        bool operator != ( const Rc& other ) const { return m_pObject != other.m_pObject; }

        bool operator == ( T *pOther ) const { return m_pObject == pOther; }
        bool operator != ( T *pOther ) const { return m_pObject == pOther; }

        bool operator == ( std::nullptr_t ) const { return m_pObject == nullptr; }
        bool operator != ( std::nullptr_t ) const { return m_pObject != nullptr; }

    private:
        T* m_pObject = nullptr;

        inline void IncRef() const
        {
            if ( m_pObject != nullptr )
                m_pObject->IncRef();
        }

        inline void DecRef() const
        {
            if ( m_pObject != nullptr )
            {
                if ( m_pObject->DecRef() == 0 )
                {
                    if constexpr ( m_pObject->ReferenceOwnsObject() )
                        delete m_pObject;
                }
            }
        }
    };
}