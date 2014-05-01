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

template <typename Type> inline void AssertImplementation(char const *File, char const *Function, int Line, Type const &Value)
{
#ifndef NDEBUG
	if (!Value)
	{
		AssertStamp(File, Function, Line);
		std::cerr << "Value was " << (bool)Value << std::endl;
		throw false;
	}
#endif
}

template <typename GotType, typename ExpectedType> inline void AssertImplementation(char const *File, char const *Function, int Line, GotType const &Got, ExpectedType const &Expected)
{
#ifndef NDEBUG
	if (Got != Expected)
	{
		AssertStamp(File, Function, Line);
		std::cerr << "Got '" << Got << "', expected '" << Expected << "'" << std::endl;
		throw false;
	}
#endif
}

#define Assert(...) AssertImplementation(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#endif
