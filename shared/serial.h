#ifndef serial_h
#define serial_h

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

#include <map>
#include <stack>

#include "type.h"

namespace Serial
{

struct WriteObjectT;
struct WritePrepolymorphT;

struct WriteArrayT
{
	public:
		WriteArrayT(WriteArrayT &&Other);
		~WriteArrayT(void);
		
		void Bool(bool const &Value);
		void Int(int64_t const &Value);
		void UInt(uint64_t const &Value);
		void Float(float const &Value);
		void String(std::string const &Value);
		void Binary(uint8_t const *Bytes, size_t const Length);
		template <typename IntT, size_t Length, typename = typename std::enable_if<std::is_integral<IntT>::value>::type> 
			void Binary(std::string const &Key, std::array<IntT, Length> const &Value)
			{ Binary(Key, reinterpret_cast<uint8_t const *>(&Value[0]), sizeof(IntT) * Value.size()); }
		WriteObjectT Object(void);
		WriteArrayT Array(void);
		WritePrepolymorphT Polymorph(void);
		
	friend struct WriteObjectT;
	protected:
		WriteArrayT(yajl_gen Base);
		
		yajl_gen Base;
};

struct WriteObjectT
{
	public:
		WriteObjectT(WriteObjectT &&Other);
		~WriteObjectT(void);
		
		void Bool(std::string const &Key, bool const &Value);
		void Int(std::string const &Key, int64_t const &Value);
		void UInt(std::string const &Key, uint64_t const &Value);
		void Float(std::string const &Key, float const &Value);
		void String(std::string const &Key, std::string const &Value);
		void Binary(std::string const &Key, uint8_t const *Bytes, size_t const Length);
		template <typename IntT, size_t Length, typename = typename std::enable_if<std::is_integral<IntT>::value>::type> 
			void Binary(std::string const &Key, std::array<IntT, Length> const &Value)
			{ Binary(Key, reinterpret_cast<uint8_t const *>(&Value[0]), sizeof(IntT) * Value.size()); }
		WriteObjectT Object(std::string const &Key);
		WriteArrayT Array(std::string const &Key);
		WritePrepolymorphT Polymorph(std::string const &Key);
		
	friend struct WriteArrayT;
	friend struct WriteT;
	protected:
		WriteObjectT(yajl_gen Base);
		
		yajl_gen Base;
};

struct WritePrepolymorphT : private WriteArrayT
{
	friend struct WriteArrayT;
	friend struct WriteObjectT;
	protected:
		using WriteArrayT::WriteArrayT;

		//operator WriteArrayT &&(void);
};

struct WritePolymorphInjectT // The C++ way: working around initialization syntactical limitations
{
	WritePolymorphInjectT(void);
	WritePolymorphInjectT(std::string const &Tag, WriteArrayT &Array);
};

struct WritePolymorphT : private WriteArrayT, private WritePolymorphInjectT, WriteObjectT
{
	WritePolymorphT(std::string const &Tag, WritePrepolymorphT &&Other);
	WritePolymorphT(WritePolymorphT &&Other);

	// Screw C++
	using WriteObjectT::Bool;
	using WriteObjectT::Int;
	using WriteObjectT::UInt;
	using WriteObjectT::Float;
	using WriteObjectT::String;
	using WriteObjectT::Binary;
	using WriteObjectT::Object;
	using WriteObjectT::Array;
	using WriteObjectT::Polymorph;
};

struct WriteT
{
	//WriteT(std::string const &Filename);
	WriteT(void);
	~WriteT(void);

	WriteObjectT Object(void);
	std::string Dump(void);

	private:
		yajl_gen Base;
};

struct ReadArrayT;
struct ReadObjectT;

typedef std::function<void(bool Value)> LooseBoolCallbackT;
typedef std::function<void(int64_t Value)> LooseIntCallbackT;
typedef std::function<void(uint64_t Value)> LooseUIntCallbackT;
typedef std::function<void(float Value)> LooseFloatCallbackT;
typedef std::function<void(std::string &&Value)> LooseStringCallbackT;
typedef std::function<void(std::vector<uint8_t> &&Value)> LooseBinaryCallbackT;
typedef std::function<void(ReadObjectT &Value)> LooseObjectCallbackT;
typedef std::function<void(ReadArrayT &Value)> LooseArrayCallbackT;

typedef StrictType(LooseBoolCallbackT) BoolCallbackT;
typedef StrictType(LooseIntCallbackT) IntCallbackT;
typedef StrictType(LooseUIntCallbackT) UIntCallbackT;
typedef StrictType(LooseFloatCallbackT) FloatCallbackT;
typedef StrictType(LooseStringCallbackT) StringCallbackT;
typedef StrictType(LooseBinaryCallbackT) BinaryCallbackT;
typedef StrictType(LooseObjectCallbackT) ObjectCallbackT;
typedef StrictType(LooseArrayCallbackT) ArrayCallbackT;

struct ReadNestableT
{
	public:
		virtual ~ReadNestableT(void);
		
	friend struct ReadT;
	protected:
		// Stack context sensitive callbacks
		virtual bool Bool(bool Value) = 0;
		virtual bool Number(std::string const &Source) = 0;
		virtual bool StringOrBinary(std::string const &Source) = 0;
		virtual bool Object(ReadObjectT &Object) = 0;
		virtual bool Key(std::string const &Value) = 0;
		virtual bool Array(ReadArrayT &Array) = 0;
};

struct ReadArrayT : ReadNestableT
{
	public:
		~ReadArrayT(void);
		
		void Bool(LooseBoolCallbackT const &Callback);
		void Int(LooseIntCallbackT const &Callback);
		void UInt(LooseUIntCallbackT const &Callback);
		void Float(LooseFloatCallbackT const &Callback);
		void String(LooseStringCallbackT const &Callback);
		void Binary(LooseBinaryCallbackT const &Callback);
		void Object(LooseObjectCallbackT const &Callback);
		void Array(LooseArrayCallbackT const &Callback);
	
		void Destructor(std::function<void(void)> const &Callback);
	
	protected:
		bool Bool(bool Value) override;
		bool Number(std::string const &Source) override;
		bool StringOrBinary(std::string const &Source) override;
		bool Object(ReadObjectT &Object) override;
		bool Key(std::string const &Value) override;
		bool Array(ReadArrayT &Array) override;
	
	private:
		VariantT
		<
			BoolCallbackT, 
			IntCallbackT, 
			UIntCallbackT, 
			FloatCallbackT, 
			StringCallbackT,
			BinaryCallbackT,
			ObjectCallbackT,
			ArrayCallbackT
		> Callback;
		std::function<void(void)> DestructorCallback;
};

struct ReadObjectT : ReadNestableT
{
	public:
		~ReadObjectT(void);
		
		void Bool(std::string const &Key, LooseBoolCallbackT const &Callback);
		void Int(std::string const &Key, LooseIntCallbackT const &Callback);
		void UInt(std::string const &Key, LooseUIntCallbackT const &Callback);
		void Float(std::string const &Key, LooseFloatCallbackT const &Callback);
		void String(std::string const &Key, LooseStringCallbackT const &Callback);
		void Binary(std::string const &Key, LooseBinaryCallbackT const &Callback);
		void Object(std::string const &Key, LooseObjectCallbackT const &Callback);
		void Array(std::string const &Key, LooseArrayCallbackT const &Callback);
		
		void Destructor(std::function<void(void)> const &Callback);
		
	protected:
		bool Bool(bool Value) override;
		bool Number(std::string const &Source) override;
		bool StringOrBinary(std::string const &Source) override;
		bool Object(ReadObjectT &Object) override;
		bool Key(std::string const &Value) override;
		bool Array(ReadArrayT &Array) override;
		
	private:
		std::string LastKey;
		std::map
		<
			std::string, 
			VariantT
			<
				BoolCallbackT, 
				IntCallbackT, 
				UIntCallbackT, 
				FloatCallbackT, 
				StringCallbackT,
				BinaryCallbackT,
				ObjectCallbackT,
				ArrayCallbackT
			>
		> Callbacks;
		std::function<void(void)> DestructorCallback;
};

struct ReadT : ReadObjectT
{
	public:
		ReadT(ObjectCallbackT const &Setup);
		~ReadT(void);
	private:
		yajl_handle Base;
		std::stack<std::unique_ptr<ReadNestableT>> Stack;
};

}

#endif
