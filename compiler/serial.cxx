#include "serial.h"

#include <cstring>

#include "extrastandard.h"
#include "math.h"

static char const StringPrefix[] = "utf8:";
static char const BinaryPrefix[] = "alpha16:";

static std::vector<char> ToString(std::string const &In)
{
	std::vector<char> Out;
	Out.resize(5 + In.length());
	memcpy(&Out[0], StringPrefix, sizeof(StringPrefix) - 1);
	memcpy(&Out[sizeof(StringPrefix) - 1], In.c_str(), In.length());
	return Out;
}

static std::vector<char> ToBinary(uint8_t const *Bytes, size_t const Length)
{
	std::vector<char> Out;
	Out.resize(5 + Length * 2);
	memcpy(&Out[0], BinaryPrefix, sizeof(BinaryPrefix) - 1);
	for (size_t Index = 0; Index < Length; ++Index)
	{
		Out[sizeof(BinaryPrefix) - 1 + Index * 2] = (Bytes[Index] >> 8) + 'a';
		Out[sizeof(BinaryPrefix) - 1 + Index * 2 + 1] = (Bytes[Index] & 0xf) + 'a';
	}
	return Out;
}

static std::vector<uint8_t> FromBinary(std::string const &In)
{
	if (In.size() % 2 != 0) return {};
	std::vector<uint8_t> Out(In.size() / 2);
	for (size_t Position = 0; Position < In.size() / 2; ++Position)
	{
		if (!RangeT<char>('a', 16).Contains(In[Position * 2])) return {};
		if (!RangeT<char>('a', 16).Contains(In[Position * 2 + 1])) return {};
		Out[Position] = 
			((In[Position * 2] - 'a') << 8) +
			(In[Position * 2 + 1] - 'a');
	}
	return Out;
}

namespace Serial
{

//================================================================================================================
// Writing

//----------------------------------------------------------------------------------------------------------------
// Array writer
WriteArrayT::WriteArrayT(WriteArrayT &&Other) : Base(Other.Base) { Other.Base = nullptr; }

WriteArrayT::~WriteArrayT(void) { if (Base) yajl_gen_array_close(Base); }

void WriteArrayT::Bool(bool const &Value) { Assert(Base); if (Base) yajl_gen_bool(Base, Value); }

void WriteArrayT::Int(int64_t const &Value) { Assert(Base); if (Base) yajl_gen_integer(Base, Value); }

void WriteArrayT::UInt(uint64_t const &Value) { Assert(Base); if (Base) yajl_gen_integer(Base, Value); }

void WriteArrayT::Float(float const &Value) { Assert(Base); if (Base) yajl_gen_double(Base, Value); }

void WriteArrayT::String(std::string const &Value) 
{
	Assert(Base); 
	if (Base) 
	{
		auto Temp = ToString(Value);
		yajl_gen_string(Base, reinterpret_cast<unsigned char *>(&Temp[0]), Temp.size()); 
	}
}

void WriteArrayT::Binary(uint8_t const *Bytes, size_t const Length) 
{
	Assert(Base); 
	if (Base) 
	{
		auto Temp = ToBinary(Bytes, Length);
		yajl_gen_string(Base, reinterpret_cast<unsigned char *>(&Temp[0]), Temp.size()); 
	}
}

WriteObjectT WriteArrayT::Object(void) { return WriteObjectT(Base); }

WriteArrayT WriteArrayT::Array(void) { return WriteArrayT(Base); }

WriteArrayT::WriteArrayT(yajl_gen Base) : Base(Base) { Assert(Base); if (Base) yajl_gen_array_open(Base); }

//----------------------------------------------------------------------------------------------------------------
// Object writer
WriteObjectT::WriteObjectT(WriteObjectT &&Other) : Base(Other.Base) { Other.Base = nullptr; }

WriteObjectT::~WriteObjectT(void) { if (Base) yajl_gen_map_close(Base); }

void WriteObjectT::Bool(std::string const &Key, bool const &Value) 
{ 
	if (Base) 
	{
		yajl_gen_string(Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
		yajl_gen_bool(Base, Value); 
	} 
	else Assert(false);
}

void WriteObjectT::Int(std::string const &Key, int64_t const &Value)
{ 
	if (Base) 
	{
		yajl_gen_string(Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
		yajl_gen_integer(Base, Value); 
	} 
	else Assert(false);
}

void WriteObjectT::UInt(std::string const &Key, uint64_t const &Value)
{ 
	if (Base) 
	{
		yajl_gen_string(Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
		yajl_gen_integer(Base, Value); 
	} 
	else Assert(false);
}

void WriteObjectT::Float(std::string const &Key, float const &Value)
{ 
	if (Base) 
	{
		yajl_gen_string(Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
		yajl_gen_double(Base, Value); 
	} 
	else Assert(false);
}

void WriteObjectT::String(std::string const &Key, std::string const &Value) 
{ 
	if (Base) 
	{
		yajl_gen_string(Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
		auto Temp = ToString(Value);
		yajl_gen_string(Base, reinterpret_cast<unsigned char *>(&Temp[0]), Temp.size()); 
	} 
	else Assert(false);
}

void WriteObjectT::Binary(std::string const &Key, uint8_t const *Bytes, size_t const Length) 
{ 
	if (Base) 
	{
		yajl_gen_string(Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
		auto Temp = ToBinary(Bytes, Length);
		yajl_gen_string(Base, reinterpret_cast<unsigned char *>(&Temp[0]), Temp.size()); 
	} 
	else Assert(false);
}

WriteObjectT WriteObjectT::Object(std::string const &Key)
{ 
	if (Base) yajl_gen_string(Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
	else Assert(false);
	return WriteObjectT(Base);
}

WriteArrayT WriteObjectT::Array(std::string const &Key)
{ 
	if (Base) yajl_gen_string(Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
	else Assert(false);
	return WriteArrayT(Base);
}

WriteObjectT::WriteObjectT(yajl_gen Base) : Base(Base) { Assert(Base); if (Base) yajl_gen_map_open(Base); }

//----------------------------------------------------------------------------------------------------------------
// Writing start point
WriteT::WriteT(void) : WriteObjectT(yajl_gen_alloc(nullptr))
{
}

WriteT::~WriteT(void)
{
	yajl_gen_map_close(Base);
	yajl_gen_free(Base);
	Base = nullptr;
}

//================================================================================================================
// Reading

ReadNestableT::~ReadNestableT(void) {}

//----------------------------------------------------------------------------------------------------------------
// Nested array reader
ReadArrayT::~ReadArrayT(void) { if (DestructorCallback) DestructorCallback(); }

void ReadArrayT::Bool(LooseBoolCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<BoolCallbackT>(Callback); }
void ReadArrayT::Int(LooseIntCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<IntCallbackT>(Callback); }
void ReadArrayT::UInt(LooseUIntCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<UIntCallbackT>(Callback); }
void ReadArrayT::Float(LooseFloatCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<FloatCallbackT>(Callback); }
void ReadArrayT::String(LooseStringCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<StringCallbackT>(Callback); }
void ReadArrayT::Binary(LooseBinaryCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<BinaryCallbackT>(Callback); }
void ReadArrayT::Object(LooseObjectCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<ObjectCallbackT>(Callback); }
void ReadArrayT::Array(LooseArrayCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<ArrayCallbackT>(Callback); }

void ReadArrayT::Destructor(std::function<void(void)> const &Callback) { Assert(!DestructorCallback); DestructorCallback = Callback; }

bool ReadArrayT::Bool(bool Value) 
	{ if (Callback.Is<BoolCallbackT>()) Callback.Get<BoolCallbackT>()(Value); return true; }
	
bool ReadArrayT::Number(std::string const &Source)
{
	// TODO handle scientific/eX notation somehow?
	if (Callback.Is<IntCallbackT>())
	{
		int64_t Value;
		if (!(StringT(Source) >> Value)) return false;
		Callback.Get<IntCallbackT>()(Value);
	}
	else if (Callback.Is<UIntCallbackT>())
	{
		uint64_t Value;
		if (!(StringT(Source) >> Value)) return false;
		Callback.Get<UIntCallbackT>()(Value);
	}
	else if (Callback.Is<FloatCallbackT>())
	{
		float Value;
		if (!(StringT(Source) >> Value)) return false;
		Callback.Get<FloatCallbackT>()(Value);
	}
	return true;
}

bool ReadArrayT::StringOrBinary(std::string const &Source)
{
	if (Source.substr(0, sizeof(StringPrefix) - 1) == StringPrefix)
	{
		if (Callback.Is<StringCallbackT>()) 
			Callback.Get<StringCallbackT>()(Source.substr(sizeof(StringPrefix) - 1));
	}
	else if (Source.substr(0, sizeof(BinaryPrefix) - 1) == BinaryPrefix)
	{
		if (Callback.Is<BinaryCallbackT>()) 
			Callback.Get<BinaryCallbackT>()(FromBinary(Source.substr(sizeof(BinaryPrefix) - 1)));
	}
	else return false; // Invalid string notation
	return true;
}

bool ReadArrayT::Object(ReadObjectT &Object)
	{ if (Callback.Is<ObjectCallbackT>()) Callback.Get<ObjectCallbackT>()(std::ref(Object)); return true; }

bool ReadArrayT::Key(std::string const &Value)
	{ return false; } // Keys shouldn't appear in arrays.  Hopefully yajl will catch this first.
	
bool ReadArrayT::Array(ReadArrayT &Array)
	{ if (Callback.Is<ArrayCallbackT>()) Callback.Get<ArrayCallbackT>()(std::ref(Array)); return true; }

//----------------------------------------------------------------------------------------------------------------
// Nested object reader
ReadObjectT::~ReadObjectT(void) { if (DestructorCallback) DestructorCallback(); }

void ReadObjectT::Bool(std::string const &Key, LooseBoolCallbackT const &Callback) 
	{ Assert(!Callbacks[Key]); Callbacks[Key].Set<BoolCallbackT>(Callback); }
void ReadObjectT::Int(std::string const &Key, LooseIntCallbackT const &Callback) 
	{ Assert(!Callbacks[Key]); Callbacks[Key].Set<IntCallbackT>(Callback); }
void ReadObjectT::UInt(std::string const &Key, LooseUIntCallbackT const &Callback) 
	{ Assert(!Callbacks[Key]); Callbacks[Key].Set<UIntCallbackT>(Callback); }
void ReadObjectT::Float(std::string const &Key, LooseFloatCallbackT const &Callback) 
	{ Assert(!Callbacks[Key]); Callbacks[Key].Set<FloatCallbackT>(Callback); }
void ReadObjectT::String(std::string const &Key, LooseStringCallbackT const &Callback) 
	{ Assert(!Callbacks[Key]); Callbacks[Key].Set<StringCallbackT>(Callback); }
void ReadObjectT::Binary(std::string const &Key, LooseBinaryCallbackT const &Callback) 
	{ Assert(!Callbacks[Key]); Callbacks[Key].Set<BinaryCallbackT>(Callback); }
void ReadObjectT::Object(std::string const &Key, LooseObjectCallbackT const &Callback) 
	{ Assert(!Callbacks[Key]); Callbacks[Key].Set<ObjectCallbackT>(Callback); }
void ReadObjectT::Array(std::string const &Key, LooseArrayCallbackT const &Callback) 
	{ Assert(!Callbacks[Key]); Callbacks[Key].Set<ArrayCallbackT>(Callback); }

void ReadObjectT::Destructor(std::function<void(void)> const &Callback) { Assert(!DestructorCallback); DestructorCallback = Callback; }

bool ReadObjectT::Bool(bool Value) 
{ 
	if (LastKey.empty()) return false; 
	auto Callback = Callbacks.find(LastKey);
	if (Callback != Callbacks.end())
		Callback->second.Get<BoolCallbackT>()(Value);
	LastKey.clear();
	return true;
}
	
bool ReadObjectT::Number(std::string const &Source)
{ 
	if (LastKey.empty()) return false; 
	auto Callback = Callbacks.find(LastKey);
	LastKey.clear();
	if (Callback != Callbacks.end())
	{
		if (Callback->second.Is<IntCallbackT>())
		{
			int64_t Value;
			if (!(StringT(Source) >> Value)) return false;
			Callback->second.Get<IntCallbackT>()(Value);
		}
		else if (Callback->second.Is<UIntCallbackT>())
		{
			uint64_t Value;
			if (!(StringT(Source) >> Value)) return false;
			Callback->second.Get<UIntCallbackT>()(Value);
		}
		else if (Callback->second.Is<FloatCallbackT>())
		{
			float Value;
			if (!(StringT(Source) >> Value)) return false;
			Callback->second.Get<FloatCallbackT>()(Value);
		}
	}
	return true;
}

bool ReadObjectT::StringOrBinary(std::string const &Source)
{ 
	if (LastKey.empty()) return false; 
	auto Callback = Callbacks.find(LastKey);
	LastKey.clear();
	if (Source.substr(0, sizeof(StringPrefix) - 1) == StringPrefix)
	{
		if ((Callback != Callbacks.end()) && Callback->second.Is<StringCallbackT>()) 
			Callback->second.Get<StringCallbackT>()(Source.substr(sizeof(StringPrefix) - 1));
	}
	else if (Source.substr(0, sizeof(BinaryPrefix) - 1) == BinaryPrefix)
	{
		if ((Callback != Callbacks.end()) && Callback->second.Is<BinaryCallbackT>()) 
			Callback->second.Get<BinaryCallbackT>()(FromBinary(Source.substr(sizeof(BinaryPrefix) - 1)));
	}
	else return false; // Invalid string notation
	return true;
}

bool ReadObjectT::Object(ReadObjectT &Object)
{ 
	if (LastKey.empty()) return false; 
	auto Callback = Callbacks.find(LastKey);
	if ((Callback != Callbacks.end()) && Callback->second.Is<ObjectCallbackT>())
		Callback->second.Get<ObjectCallbackT>()(std::ref(Object));
	LastKey.clear();
	return true;
}

bool ReadObjectT::Key(std::string const &Value) { LastKey = Value; return true; }

bool ReadObjectT::Array(ReadArrayT &Array)
{ 
	if (LastKey.empty()) return false; 
	auto Callback = Callbacks.find(LastKey);
	if ((Callback != Callbacks.end()) && Callback->second.Is<ArrayCallbackT>())
		Callback->second.Get<ArrayCallbackT>()(std::ref(Array));
	LastKey.clear();
	return true;
}

//----------------------------------------------------------------------------------------------------------------
// Nested object reader
ReadT::ReadT(ObjectCallbackT const &Setup)
{
	static auto PrepareUserData = [](void *UserData) -> OptionalT<ReadT *>
	{
		auto This = reinterpret_cast<ReadT *>(UserData);
		if (This->Stack.empty()) return {};
		return This;
	};
	
	// Assuming yajl enforces json correctness, so start map start/end are matched, open/closes aren't crossed, etc
	static yajl_callbacks Callbacks
	{  
		// Primitives
		nullptr,  
		[](void *UserData, int Value) -> int // Bool
		{
			auto This = PrepareUserData(UserData); 
			if (!This) return false;
			if (!This->Stack.top()->Bool(Value)) return false;
			return true;
		},
		nullptr,
		nullptr,
		[](void *UserData, char const *Value, size_t ValueLength) -> int // Number
		{
			auto This = PrepareUserData(UserData); 
			if (!This) return false;
			if (!This->Stack.top()->Number(std::string(Value, ValueLength))) return false;
			return true;
		},
		[](void *UserData, unsigned char const *Value, size_t ValueLength) -> int // String/Binary
		{
			auto This = PrepareUserData(UserData); 
			if (!This) return false;
			if (!This->Stack.top()->StringOrBinary(std::string(reinterpret_cast<char const *>(Value), ValueLength))) return false;
			return true;
		},
		
		// Object
		[](void *UserData) -> int // Open
		{
			auto This = PrepareUserData(UserData); 
			if (!This) return false;
			auto NewTop = new ReadObjectT;
			if (!This->Object(*NewTop)) return false;
			This->Stack.push(std::unique_ptr<ReadNestableT>(NewTop));
			return true;
		},
		[](void *UserData, unsigned char const *Key, size_t KeyLength) -> int // Key
		{
			auto This = PrepareUserData(UserData); 
			if (!This) return false;
			if (!This->Stack.top()->Key(std::string(reinterpret_cast<char const *>(Key), KeyLength))) return false;
			return true;
		},
		[](void *UserData) -> int // Close
		{
			auto This = PrepareUserData(UserData); 
			if (!This) return false;
			This->Stack.pop();
			return true;
		},
		
		// Array
		[](void *UserData) -> int // Open
		{
			auto This = PrepareUserData(UserData); 
			if (!This) return false;
			auto NewTop = new ReadArrayT;
			if (!This->Array(*NewTop)) return false;
			This->Stack.push(std::unique_ptr<ReadNestableT>(NewTop));
			return true;
		},
		[](void *UserData) -> int // Close
		{
			auto This = PrepareUserData(UserData); 
			if (!This) return false;
			This->Stack.pop();
			return true;
		}
	};
	Base = yajl_alloc(&Callbacks, NULL, this);  
	
	auto NewTop = new ReadObjectT;
	Setup(std::ref(*NewTop));
	Stack.push(std::unique_ptr<ReadNestableT>(NewTop));
}

ReadT::~ReadT(void)
{
	yajl_free(Base);
}

}
