#ifndef serial_h
#define serial_h

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

#include <map>
#include <stack>

#include "type.h"
#include "filesystem.h"

namespace Serial
{

struct WriteObjectT;
struct WritePrepolymorphT;

struct WriteCoreT
{
	WriteCoreT(yajl_gen Base);
	virtual ~WriteCoreT(void);
	yajl_gen Base;
};

struct WriteArrayT
{
	public:
		WriteArrayT(WriteArrayT &&Other);
		
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
		WriteArrayT(std::shared_ptr<WriteCoreT> ParentCore);
		std::shared_ptr<WriteCoreT> ParentCore, Core;
};

struct WriteObjectT
{
	public:
		WriteObjectT(WriteObjectT &&Other);
		
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
		WriteObjectT(std::shared_ptr<WriteCoreT> ParentCore);
		std::shared_ptr<WriteCoreT> ParentCore, Core;
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

	WriteObjectT Object(void);
	std::string Dump(void);
	void Dump(Filesystem::PathT const &Path);

	private:
		std::shared_ptr<WriteCoreT> Core;
};

typedef OptionalT<std::string> ReadErrorT;
struct ReadArrayT;
struct ReadObjectT;

// TODO Return ReadErrorT from all callbacks, propagate and halt parsing
typedef std::function<ReadErrorT(bool Value)> LooseBoolCallbackT;
typedef std::function<ReadErrorT(int64_t Value)> LooseIntCallbackT;
typedef std::function<ReadErrorT(uint64_t Value)> LooseUIntCallbackT;
typedef std::function<ReadErrorT(float Value)> LooseFloatCallbackT;
typedef std::function<ReadErrorT(std::string &&Value)> LooseStringCallbackT;
typedef std::function<ReadErrorT(std::vector<uint8_t> &&Value)> LooseBinaryCallbackT;
typedef std::function<ReadErrorT(ReadObjectT &Value)> LooseObjectCallbackT;
typedef std::function<ReadErrorT(ReadArrayT &Value)> LooseArrayCallbackT;
typedef std::function<ReadErrorT(std::string &&Type, ReadObjectT &Value)> LoosePolymorphCallbackT;

typedef StrictType(LooseBoolCallbackT) BoolCallbackT;
typedef StrictType(LooseIntCallbackT) IntCallbackT;
typedef StrictType(LooseUIntCallbackT) UIntCallbackT;
typedef StrictType(LooseFloatCallbackT) FloatCallbackT;
typedef StrictType(LooseStringCallbackT) StringCallbackT;
typedef StrictType(LooseBinaryCallbackT) BinaryCallbackT;
typedef StrictType(LooseObjectCallbackT) ObjectCallbackT;
typedef StrictType(LooseArrayCallbackT) ArrayCallbackT;
typedef StrictType(LoosePolymorphCallbackT) PolymorphCallbackT;
struct InternalPolymorphCallbackT
{
	LooseStringCallbackT StringCallback;
	LooseObjectCallbackT ObjectCallback;
};
			
typedef VariantT
	<
		BoolCallbackT, 
		IntCallbackT, 
		UIntCallbackT, 
		FloatCallbackT, 
		StringCallbackT,
		BinaryCallbackT,
		ObjectCallbackT,
		ArrayCallbackT,
		PolymorphCallbackT,
		InternalPolymorphCallbackT
	>
	ReadCallbackVariantT;

struct ReadNestableT
{
	public:
		virtual ~ReadNestableT(void);
		
	friend struct ReadT;
	protected:
		// Stack context sensitive callbacks
		virtual ReadErrorT Bool(bool Value) = 0;
		virtual ReadErrorT Number(std::string const &Source) = 0;
		virtual ReadErrorT StringOrBinary(std::string const &Source) = 0;
		virtual ReadErrorT Object(ReadObjectT &Object) = 0;
		virtual ReadErrorT Key(std::string const &Value) = 0;
		virtual ReadErrorT Array(ReadArrayT &Array) = 0;
		virtual ReadErrorT Final(void) = 0;
};

struct ReadArrayT : ReadNestableT
{
	public:
		void Bool(LooseBoolCallbackT const &Callback);
		void Int(LooseIntCallbackT const &Callback);
		void UInt(LooseUIntCallbackT const &Callback);
		void Float(LooseFloatCallbackT const &Callback);
		void String(LooseStringCallbackT const &Callback);
		void Binary(LooseBinaryCallbackT const &Callback);
		void Object(LooseObjectCallbackT const &Callback);
		void Array(LooseArrayCallbackT const &Callback);
		void Polymorph(LoosePolymorphCallbackT const &Callback);
	
		void Finally(std::function<ReadErrorT(void)> const &Callback);
	
	protected:
		friend ReadErrorT ReadPolymorph(ReadCallbackVariantT &, ReadArrayT &, bool);
		void InternalPolymorph(InternalPolymorphCallbackT const &Callback);
		ReadErrorT Bool(bool Value) override;
		ReadErrorT Number(std::string const &Source) override;
		ReadErrorT StringOrBinary(std::string const &Source) override;
		ReadErrorT Object(ReadObjectT &Object) override;
		ReadErrorT Key(std::string const &Value) override;
		ReadErrorT Array(ReadArrayT &Array) override;
		ReadErrorT Final(void) override;
	
	private:
		ReadCallbackVariantT Callback;
		std::function<ReadErrorT(void)> DestructorCallback;
};

struct ReadObjectT : ReadNestableT
{
	public:
		void Bool(std::string const &Key, LooseBoolCallbackT const &Callback);
		void Int(std::string const &Key, LooseIntCallbackT const &Callback);
		void UInt(std::string const &Key, LooseUIntCallbackT const &Callback);
		void Float(std::string const &Key, LooseFloatCallbackT const &Callback);
		void String(std::string const &Key, LooseStringCallbackT const &Callback);
		void Binary(std::string const &Key, LooseBinaryCallbackT const &Callback);
		void Object(std::string const &Key, LooseObjectCallbackT const &Callback);
		void Array(std::string const &Key, LooseArrayCallbackT const &Callback);
		void Polymorph(std::string const &Key, LoosePolymorphCallbackT const &Callback);
		
		void Finally(std::function<ReadErrorT(void)> const &Callback);
		
	protected:
		ReadErrorT Bool(bool Value) override;
		ReadErrorT Number(std::string const &Source) override;
		ReadErrorT StringOrBinary(std::string const &Source) override;
		ReadErrorT Object(ReadObjectT &Object) override;
		ReadErrorT Key(std::string const &Value) override;
		ReadErrorT Array(ReadArrayT &Array) override;
		ReadErrorT Final(void) override;
		
	private:
		std::string LastKey;
		std::map<std::string, ReadCallbackVariantT> Callbacks;
		std::function<ReadErrorT(void)> DestructorCallback;
};

struct ReadT : ReadArrayT
{
	public:
		ReadT(void);
		~ReadT(void);
		ReadErrorT Parse(Filesystem::PathT const &Path);
		ReadErrorT Parse(std::istream &Stream);
		ReadErrorT Parse(std::istream &&Stream); // C++ IS SO AWESOME
	private:
		yajl_handle Base;
		std::stack<std::unique_ptr<ReadNestableT, void(*)(ReadNestableT *)>> Stack;
		ReadErrorT Error;
};

}

#endif
