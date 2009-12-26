#include <stdio.h>
#include <stdarg.h>

#include "Macros.h"
#include "String.h"

namespace Finch
{
    const char * String::sEmptyString = "";
    
    String String::Format(const String & format, ...)
    {
        char result[FormattedStringMax];
        
        va_list args;
        va_start (args, format);
        
        vsprintf(result, format.CString(), args);

        va_end (args);
        
        return String(result);
    }
    
    String::String(const char* chars)
    {
        // hoist it onto the heap
        int length = strlen(chars);
        char* heap = new char[length + 1];
        strcpy(heap, chars);
        
        mData = Ref<StringData>(new StringData(heap));
    }
    
    String::String(char c)
    {
        char chars[2];
        chars[0] = c;
        chars[1] = '\0';
        
        // hoist it onto the heap
        int length = strlen(chars);
        char* heap = new char[length + 1];
        strcpy(heap, chars);
        
        mData = Ref<StringData>(new StringData(heap));
    }
    
    bool String::operator <(const String & other) const
    {
        return strcmp(CString(), other.CString()) < 0;
    }
    
    bool String::operator <=(const String & other) const
    {
        return strcmp(CString(), other.CString()) <= 0;
    }
    
    bool String::operator >(const String & other) const
    {
        return strcmp(CString(), other.CString()) > 0;
    }
    
    bool String::operator >=(const String & other) const
    {
        return strcmp(CString(), other.CString()) >= 0;
    }
    
    bool String::operator ==(const String & other) const
    {
        if (this == &other) return true;
        if (mData == other.mData) return true;
        
        return strcmp(CString(), other.CString()) == 0;
    }
    
    bool String::operator !=(const String & other) const
    {
        return !(*this == other);
    }
    
    const char & String::operator[] (int index) const
    {
        ASSERT_INDEX(index, Length() + 1); // allow accessing the terminator

        if (mData.IsNull()) return sEmptyString[0];
        return mData->chars[index];
    }
    
    String String::operator +(const String & other) const
    {
        return String(*this, other);
    }
    
    String & String::operator +=(const String & other)
    {
        *this = *this + other;
        return *this;
    }

    const char* String::CString() const
    {
        if (mData.IsNull()) return sEmptyString;
        
        return mData->chars;
    }

    int String::Length() const
    {
        if (mData.IsNull()) return 0;
        
        return mData->length;
    }

    String::String(const String & left, const String & right)
    {
        // make a new buffer on the heap
        int length = left.Length() + right.Length();
        char* heap = new char[length + 1];
        
        // concatenate the strings
        strcpy(heap, left.CString());
        strcpy(&heap[left.Length()], right.CString());
        
        mData = Ref<StringData>(new StringData(heap));
    }
    
    bool operator ==(const char * left, const String & right)
    {
        return strcmp(left, right.CString()) == 0;
    }
    
    bool operator !=(const char * left, const String & right)
    {
        return strcmp(left, right.CString()) != 0;
    }
    
    bool operator ==(const String & left, const char * right)
    {
        return strcmp(left.CString(), right) == 0;
    }
    
    bool operator !=(const String & left, const char * right)
    {
        return strcmp(left.CString(), right) != 0;
    }
    
    ostream & operator <<(ostream & cout, const String & string)
    {
        cout << string.CString();
        return cout;
    }
}