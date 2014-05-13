#ifndef extrastandard_h
#define extrastandard_h

#include <memory>
#include <sstream>
#include <iostream>
#include <functional>
#include <algorithm>

//----------------------------------------------------------------------------------------------------------------
// Remove values for vector
template <typename VectorT, typename FunctionT> void VectorRemove(VectorT &Vector, FunctionT const &Filter)
	{ Vector.erase(std::remove_if(Vector.begin(), Vector.end(), Filter), Vector.end()); }

//----------------------------------------------------------------------------------------------------------------
// r-value string serialization and deserialization
struct StringT
{
	private:
		std::stringstream Buffer;
	public:

	StringT(void) {}
	StringT(std::string const &Initial) : Buffer(Initial) {}
	template <typename Whatever> StringT &operator <<(Whatever const &Input) { Buffer << Input; return *this; }
	template <typename Whatever> StringT &operator >>(Whatever &Output) { Buffer >> Output; return *this; }
	bool operator !(void) const { return !Buffer; }
	decltype(Buffer.str()) str(void) const { return Buffer.str(); }
	operator std::string(void) const { return Buffer.str(); }
};

inline std::ostream &operator <<(std::ostream &Stream, StringT const &Value)
	{ return Stream << (std::string)Value; }

//----------------------------------------------------------------------------------------------------------------
// Will be included in C++14 lolololol
template<typename T, typename... Args> std::unique_ptr<T> make_unique(Args&&... args)
	{ return std::unique_ptr<T>(new T(std::forward<Args>(args)...)); }

//----------------------------------------------------------------------------------------------------------------
// A more informative assert?
inline void AssertStamp(char const *File, char const *Function, int Line)
        { std::cerr << File << "/" << Function << ":" << Line << " Assertion failed" << std::endl; }

template <typename Type> inline void AssertImplementation(char const *File, char const *Function, int Line, char const *ValueString, Type const &Value)
{
#ifndef NDEBUG
	if (!Value)
	{
		AssertStamp(File, Function, Line);
		std::cerr << "Value (" << ValueString << ") '" << (bool)Value << "'" << std::endl;
		throw false;
	}
#endif
}

template <typename GotType, typename ExpectedType> inline void AssertImplementationE(char const *File, char const *Function, int Line, char const *GotString, GotType const &Got, char const *ExpectedString, ExpectedType const &Expected)
{
#ifndef NDEBUG
	bool Result = Got == Expected;
	if (!Result)
	{
		AssertStamp(File, Function, Line);
		std::cerr << "Got (" << GotString << ") '" << Got << "' == expected (" << ExpectedString << ") '" << Expected << "'" << std::endl;
		throw false;
	}
#endif
}

template <typename GotType, typename ExpectedType> inline void AssertImplementationNE(char const *File, char const *Function, int Line, char const *GotString, GotType const &Got, char const *ExpectedString, ExpectedType const &Expected)
{
#ifndef NDEBUG
	bool Result = Got != Expected;
	if (!Result)
	{
		AssertStamp(File, Function, Line);
		std::cerr << "Got (" << GotString << ") '" << Got << "' != expected (" << ExpectedString << ") '" << Expected << "'" << std::endl;
		throw false;
	}
#endif
}

template <typename GotType, typename ExpectedType> inline void AssertImplementationLT(char const *File, char const *Function, int Line, char const *GotString, GotType const &Got, char const *ExpectedString, ExpectedType const &Expected)
{
#ifndef NDEBUG
	bool Result = Got < Expected;
	if (!Result)
	{
		AssertStamp(File, Function, Line);
		std::cerr << "Got (" << GotString << ") '" << Got << "' < expected (" << ExpectedString << ") '" << Expected << "'" << std::endl;
		throw false;
	}
#endif
}

template <typename Got1Type, typename Got2Type> inline void AssertImplementationOr(char const *File, char const *Function, int Line, char const *Got1String, Got1Type const &Got1, char const *Got2String, Got2Type const &Got2)
{
#ifndef NDEBUG
	bool Result = Got1 || Got2;
	if (!Result)
	{
		AssertStamp(File, Function, Line);
		std::cerr << "Got #1 (" << Got1String << ") '" << Got1 << "' || got #2 (" << Got2String << ") '" << Got2 << "'" << std::endl;
		throw false;
	}
#endif
}

#define Assert(Arg1) AssertImplementation(__FILE__, __FUNCTION__, __LINE__, #Arg1, Arg1)
#define AssertE(Arg1, Arg2) AssertImplementationE(__FILE__, __FUNCTION__, __LINE__, #Arg1, Arg1, #Arg2, Arg2)
#define AssertNE(Arg1, Arg2) AssertImplementationNE(__FILE__, __FUNCTION__, __LINE__, #Arg1, Arg1, #Arg2, Arg2)
#define AssertLT(Arg1, Arg2) AssertImplementationLT(__FILE__, __FUNCTION__, __LINE__, #Arg1, Arg1, #Arg2, Arg2)
#define AssertOr(Arg1, Arg2) AssertImplementationOr(__FILE__, __FUNCTION__, __LINE__, #Arg1, Arg1, #Arg2, Arg2)

#endif
