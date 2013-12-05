#ifndef type_h
#define type_h

#include "extrastandard.h"
#include <string>
#include <sstream>

struct ConstructionError
{
	ConstructionError(void) {}
	ConstructionError(ConstructionError const &Other) : Buffer(Other.Buffer.str()) {}
	template <typename Whatever> ConstructionError &operator <<(Whatever const &Input) { Buffer << Input; return *this; }
	operator std::string(void) const { return Buffer.str(); }

	private:
		std::stringstream Buffer;
};

inline std::ostream& operator <<(std::ostream &Out, ConstructionError const &Error)
	{ Out << static_cast<std::string>(Error); return Out; }

template <typename DataType> struct Optional
{
	Optional(void) : Valid(false) {}
	Optional(DataType const &Data) : Valid(true), Data(Data) {}
	Optional &operator =(Optional<DataType> const &Other) { Valid = Other.Valid; if (Valid) Data = Other.Data; return *this; }
	operator bool(void) const { return Valid; }
	bool operator !(void) const { return !Valid; }
	DataType &operator *(void) { Assert(Valid); return Data; }
	DataType const &operator *(void) const { Assert(Valid); return Data; }
	DataType *operator ->(void) { Assert(Valid); return &Data; }
	DataType const *operator ->(void) const { Assert(Valid); return &Data; }
	bool operator ==(Optional<DataType> const &Other) const
		{ return (!Valid && !Other.Valid) || (Valid && Other.Valid && (Data == Other.Data)); }
	bool operator <(Optional<DataType> const &Other) const
	{
		if (!Valid && !Other.Valid) return false;
		if (Valid && Other.Valid) return Data < Other.Data;
		if (!Other.Valid) return true;
		return false;
	}
	bool Valid;
	DataType Data;
};

struct Pass {};
struct Fail {};

template <typename ErrorType> bool ErrorOrDefaultCheckFunction(ErrorType const &Error) { return !!Error; }
template <typename ErrorType, typename DataType, bool(*Check)(ErrorType const &) = ErrorOrDefaultCheckFunction<ErrorType>> struct ErrorOr
{
	ErrorOr(Fail, ErrorType const &Message) : Error(Message) {}
	ErrorOr(Pass, DataType const &Data) : Data(Data) {}
	ErrorOr(DataType const &Data) : Data(Data) {}
	operator bool(void) const { return Check(Error); }
	bool operator !(void) const { return !Check(Error); }
	DataType &operator *(void) { Assert(Check(Error)); return Data; }
	DataType const &operator *(void) const { Assert(Check(Error)); return Data; }
	DataType *operator ->(void) { Assert(Check(Error)); return &Data; }
	DataType const *operator ->(void) const { Assert(Check(Error)); return &Data; }
	ErrorType const Error;
	DataType Data;
};

bool TextErrorOrCheckFunction(std::string const &Error) { return !Error.empty(); }
template <typename DataT> using TextErrorOr = ErrorOr<std::string, DataT, TextErrorOrCheckFunction>;

#endif
