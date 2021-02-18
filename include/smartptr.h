#ifndef SMARTPTR_H
#define SMARTPTR_H

#pragma warning( disable : 4786 )
#pragma warning( disable : 4503 )

#ifndef NULL
#define NULL 0
#endif //NULL

#include <iostream>
#include <typeinfo>

template <class T> class SmartPtr;

//============================================================
/////////////////////////////////////////
// IRefCount
// This is the base class for reference-counting objects.
// To use it just derive your class from it:
// class CMyObject : public IRefCount { ... };

class IRefCount {
public:
    void __IncRefCount()
    {
        __m_counter++;
    }
    void __DecRefCount()
    {
        __m_counter--;
        if(__m_counter<=0)
        {
            __DestroyRef();
	}
    }

    void __DestroyRef()
    {
       //std::cerr<<"Destructor for "<<typeid(this).name()<<" ptr="<<this<<std::endl;
       delete this;
    }

    IRefCount()
    {
        __m_counter = 0;
    }
public:
//A virtual destructor is needed
    virtual ~IRefCount() {};

private:
    int __m_counter;

};

//============================================================

/////////////////////////////////////////
// SmartPtr
// Use:
// typedef SmartPtr<CObjType> CObjTypeP;


template <class T> class SmartPtr {
public:

    SmartPtr()
    {
        __ptr = NULL;
    }
    SmartPtr(T * ptr)
    {
        __ptr = NULL;
        __Assign(ptr);
    }
    SmartPtr(const SmartPtr &sp)
    {
        __ptr = NULL;
        __Assign(sp.__ptr);
    }
    virtual ~SmartPtr()
    {
        __free();
    }

    // get the contained pointer
    T *GetPtr() const
    {
	return __ptr;
    }

    // assign another smart pointer
    SmartPtr & operator = (const SmartPtr &sp) {__Assign(sp.__ptr); return *this;}
    // assign pointer or NULL
    SmartPtr & operator = (T * ptr) {__Assign(ptr); return *this;}
    // to access members of T
    T * operator ->()
    {
#ifdef _ASSERT
        _ASSERT(GetPtr()!=NULL);
#endif
        return GetPtr();
    }
    const T * operator ->() const
    {
#ifdef _ASSERT
        _ASSERT(GetPtr()!=NULL);
#endif
        return GetPtr();
    }


    // conversion to T* (for function calls)
    operator T* () const
    {
        return GetPtr();
    }

    // utilities
    bool operator !() const
    {
        return GetPtr()==NULL;
    }
    bool operator ==(const SmartPtr &sp) const
    {
        return GetPtr()==sp.GetPtr();
    }
    bool operator !=(const SmartPtr &sp) const
    {
        return GetPtr()!=sp.GetPtr();
    }
    bool operator < (const SmartPtr &sp) const
    {
	return GetPtr()<sp.GetPtr();
    }

private:
    // Assignment operation
    void __Assign(T* ptr)
    {
        if(ptr!=NULL) ptr->__IncRefCount();
        T* oldptr = __ptr;
        __ptr = ptr;
        if(oldptr!=NULL) oldptr->__DecRefCount();
    }

    void __free()
    {
	if (__ptr!=NULL)
		__ptr->__DecRefCount();
	__ptr = NULL;
    }

private:
    T* __ptr;


};

#endif
