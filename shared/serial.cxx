#include "serial.h"

#include <cstring>

#include "extrastandard.h"
#include "math.h"

static char const StringPrefix[] = "utf8:";
static char const BinaryPrefix[] = "alpha16:";

static std::vector<char> ToString(std::string const &In)
{
	std::vector<char> Out;
	Out.resize(sizeof(StringPrefix) - 1 + In.length());
	memcpy(&Out[0], StringPrefix, sizeof(StringPrefix) - 1);
	memcpy(&Out[sizeof(StringPrefix) - 1], In.c_str(), In.length());
	return Out;
}

static std::vector<char> ToBinary(uint8_t const *Bytes, size_t const Length)
{
	std::vector<char> Out;
	Out.resize(sizeof(BinaryPrefix) - 1 + Length * 2);
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
		if (In[Position * 2] < 'a') return {};
		if (In[Position * 2] >= 'a' + 16) return {};
		if (In[Position * 2 + 1] < 'a') return {};
		if (In[Position * 2 + 1] >= 'a' + 16) return {};
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

WriteCoreT::WriteCoreT(yajl_gen Base) : Base(Base) {}

WriteCoreT::~WriteCoreT(void) {}

//----------------------------------------------------------------------------------------------------------------
// Array writer
struct WriteArrayCoreT : WriteCoreT
{
	WriteArrayCoreT(yajl_gen Base) : WriteCoreT(Base) { Assert(Base); yajl_gen_array_open(Base); }
	~WriteArrayCoreT(void) { yajl_gen_array_close(Base); }
};

WriteArrayT::WriteArrayT(WriteArrayT &&Other) : ParentCore(Other.ParentCore), Core(Other.Core)
{ 
	Other.Core = nullptr;
	Other.ParentCore = nullptr;
}

void WriteArrayT::Bool(bool const &Value) { Assert(Core->Base); if (Core->Base) yajl_gen_bool(Core->Base, Value); }

void WriteArrayT::Int(int64_t const &Value) { Assert(Core->Base); if (Core->Base) yajl_gen_integer(Core->Base, Value); }

void WriteArrayT::UInt(uint64_t const &Value) { Assert(Core->Base); if (Core->Base) yajl_gen_integer(Core->Base, Value); }

void WriteArrayT::Float(float const &Value) { Assert(Core->Base); if (Core->Base) yajl_gen_double(Core->Base, Value); }

void WriteArrayT::String(std::string const &Value) 
{
	Assert(Core->Base); 
	if (Core->Base) 
	{
		auto Temp = ToString(Value);
		yajl_gen_string(Core->Base, reinterpret_cast<unsigned char *>(&Temp[0]), Temp.size()); 
	}
}

void WriteArrayT::Binary(uint8_t const *Bytes, size_t const Length) 
{
	Assert(Core->Base); 
	if (Core->Base) 
	{
		auto Temp = ToBinary(Bytes, Length);
		yajl_gen_string(Core->Base, reinterpret_cast<unsigned char *>(&Temp[0]), Temp.size()); 
	}
}

WriteObjectT WriteArrayT::Object(void) { return WriteObjectT(Core); }

WriteArrayT WriteArrayT::Array(void) { return WriteArrayT(Core); }
		
WritePrepolymorphT WriteArrayT::Polymorph(void) { return WritePrepolymorphT(Core); }

WriteArrayT::WriteArrayT(std::shared_ptr<WriteCoreT> ParentCore) : ParentCore(ParentCore), Core(std::make_shared<WriteArrayCoreT>(ParentCore->Base)) {}

//----------------------------------------------------------------------------------------------------------------
// Object writer
struct WriteObjectCoreT : WriteCoreT
{
	WriteObjectCoreT(yajl_gen Base) : WriteCoreT(Base) { yajl_gen_map_open(Base); }
	~WriteObjectCoreT(void) { yajl_gen_map_close(Base); }
};

WriteObjectT::WriteObjectT(WriteObjectT &&Other) : ParentCore(Other.ParentCore), Core(Other.Core) { Other.ParentCore = nullptr; Other.Core = nullptr; }

void WriteObjectT::Bool(std::string const &Key, bool const &Value) 
{ 
	if (Core->Base) 
	{
		yajl_gen_string(Core->Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
		yajl_gen_bool(Core->Base, Value); 
	} 
	else Assert(false);
}

void WriteObjectT::Int(std::string const &Key, int64_t const &Value)
{ 
	if (Core->Base) 
	{
		yajl_gen_string(Core->Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
		yajl_gen_integer(Core->Base, Value); 
	} 
	else Assert(false);
}

void WriteObjectT::UInt(std::string const &Key, uint64_t const &Value)
{ 
	if (Core->Base) 
	{
		yajl_gen_string(Core->Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
		yajl_gen_integer(Core->Base, Value); 
	} 
	else Assert(false);
}

void WriteObjectT::Float(std::string const &Key, float const &Value)
{ 
	if (Core->Base) 
	{
		yajl_gen_string(Core->Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
		yajl_gen_double(Core->Base, Value); 
	} 
	else Assert(false);
}

void WriteObjectT::String(std::string const &Key, std::string const &Value) 
{ 
	if (Core->Base) 
	{
		yajl_gen_string(Core->Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
		auto Temp = ToString(Value);
		yajl_gen_string(Core->Base, reinterpret_cast<unsigned char *>(&Temp[0]), Temp.size()); 
	} 
	else Assert(false);
}

void WriteObjectT::Binary(std::string const &Key, uint8_t const *Bytes, size_t const Length) 
{ 
	if (Core->Base) 
	{
		yajl_gen_string(Core->Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
		auto Temp = ToBinary(Bytes, Length);
		yajl_gen_string(Core->Base, reinterpret_cast<unsigned char *>(&Temp[0]), Temp.size()); 
	} 
	else Assert(false);
}

WriteObjectT WriteObjectT::Object(std::string const &Key)
{ 
	if (Core->Base) yajl_gen_string(Core->Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
	else Assert(false);
	return WriteObjectT(Core);
}

WriteArrayT WriteObjectT::Array(std::string const &Key)
{ 
	if (Core->Base) yajl_gen_string(Core->Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
	else Assert(false);
	return WriteArrayT(Core);
}

WritePrepolymorphT WriteObjectT::Polymorph(std::string const &Key) 
{ 
	if (Core->Base) yajl_gen_string(Core->Base, reinterpret_cast<unsigned char const *>(Key.c_str()), Key.length());
	else Assert(false);
	return WritePrepolymorphT(Core); 
}

WriteObjectT::WriteObjectT(std::shared_ptr<WriteCoreT> ParentCore) : ParentCore(ParentCore), Core(std::make_shared<WriteObjectCoreT>(ParentCore->Base)) {}
	
//----------------------------------------------------------------------------------------------------------------
// Polymorph writer
WritePolymorphInjectT::WritePolymorphInjectT(void) {}

WritePolymorphInjectT::WritePolymorphInjectT(std::string const &Tag, WriteArrayT &Array)
{
	Array.String(Tag);
}
		
WritePolymorphT::WritePolymorphT(std::string const &Tag, WritePrepolymorphT &&Other) :
	WriteArrayT(std::move(static_cast<WriteArrayT &&>(Other))),
	WritePolymorphInjectT(Tag, *this),
	WriteObjectT(WriteArrayT::Object())
	{}

WritePolymorphT::WritePolymorphT(WritePolymorphT &&Other) : 
	WriteArrayT(std::move(static_cast<WriteArrayT &>(Other))),
	WriteObjectT(std::move(static_cast<WriteObjectT &>(Other)))
	{}

//----------------------------------------------------------------------------------------------------------------
// Writing start point
struct TopWriteCoreT : WriteCoreT
{
	TopWriteCoreT(void) : WriteCoreT(yajl_gen_alloc(nullptr)) { }
	~TopWriteCoreT(void) { yajl_gen_free(Base); }
};

WriteT::WriteT(void) : Core(std::make_shared<TopWriteCoreT>())
{
	yajl_gen_config(Core->Base, yajl_gen_beautify, 1);
}

WriteObjectT WriteT::Object(void)
{
	return WriteObjectT(Core);
}

std::string WriteT::Dump(void)
{
	Assert(Core);
	if (!Core) return {};
	unsigned char const *YAJLBuffer;
	size_t YAJLBufferLength;
	yajl_gen_get_buf(Core->Base, &YAJLBuffer, &YAJLBufferLength);
	std::string Out(reinterpret_cast<char const *>(YAJLBuffer), YAJLBufferLength);
	yajl_gen_clear(Core->Base);
	return Out;
}

void WriteT::Dump(Filesystem::PathT const &Path)
{
	auto String = Dump();
	auto File = fopen(Path->Render().c_str(), "w");
	if (!Assert(File)) return; // TODO Error?
	fwrite(String.c_str(), String.size(), 1, File);
	fclose(File);
}

//================================================================================================================
// Reading

static ReadErrorT ReadNumber(ReadCallbackVariantT &Callback, std::string const &Source, bool Strict = false)
{
	// TODO handle scientific/eX notation somehow?
	if (Callback.Is<IntCallbackT>())
	{
		int64_t Value;
		if (!(StringT(Source) >> Value)) 
			return (StringT() << "Unable to convert to integer \'" << Source << "\'.").str();
		return Callback.Get<IntCallbackT>()(Value);
	}
	else if (Callback.Is<UIntCallbackT>())
	{
		uint64_t Value;
		if (!(StringT(Source) >> Value)) 
			return (StringT() << "Unable to convert to unsigned integer \'" << Source << "\'.").str();
		return Callback.Get<UIntCallbackT>()(Value);
	}
	else if (Callback.Is<FloatCallbackT>())
	{
		float Value;
		if (!(StringT(Source) >> Value)) 
			return (StringT() << "Unable to convert to float \'" << Source << "\'.").str();
		return Callback.Get<FloatCallbackT>()(Value);
	}
	else if (Strict) return std::string("Found number in restricted context with no numeric callbacks.");
	else return {};
}

ReadErrorT ReadString(ReadCallbackVariantT &Callback, std::string const &Source, bool Strict = false)
{
	if (Source.substr(0, sizeof(StringPrefix) - 1) == StringPrefix)
	{
		if (Callback.Is<StringCallbackT>()) 
			return Callback.Get<StringCallbackT>()(Source.substr(sizeof(StringPrefix) - 1));
		else if (Callback.Is<InternalPolymorphCallbackT>())
			return Callback.Get<InternalPolymorphCallbackT>().StringCallback(Source.substr(sizeof(StringPrefix) - 1));
		else if (Strict) return std::string("Found string element in a restricted context with no string handler.");
		else return {};
	}
	else if (Source.substr(0, sizeof(BinaryPrefix) - 1) == BinaryPrefix)
	{
		if (Callback.Is<BinaryCallbackT>()) 
			return Callback.Get<BinaryCallbackT>()(FromBinary(Source.substr(sizeof(BinaryPrefix) - 1)));
		else if (Strict) return std::string("Found binary element in a restricted context with no binary handler.");
		else return {};
	}
	else return (StringT() << "Strings must start with utf8: or binary:, unknown tagged string \'" << Source << "\'").str();
}

ReadErrorT ReadPolymorph(ReadCallbackVariantT &Callback, ReadArrayT &Array, bool Strict = false)
{
	auto Type = std::make_shared<std::string>();
	Array.InternalPolymorph({
		[Type](std::string &&String) -> ReadErrorT
		{
			if (!Type->empty()) return std::string("Multiple types specified for polymorph.");
			*Type = std::move(String); 
			return {};
		},
		[Type, &Callback](ReadObjectT &Object) -> ReadErrorT
		{ 
			if (Type->empty()) return std::string("No type specified for polymorph.");
			return Callback.Get<PolymorphCallbackT>()(std::move(*Type), std::ref(Object));
		}});
	// TODO detect invalid empty with Destructor?
	return {};
}

ReadNestableT::~ReadNestableT(void) {}

//----------------------------------------------------------------------------------------------------------------
// Nested array reader

void ReadArrayT::Bool(LooseBoolCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<BoolCallbackT>(Callback); }
void ReadArrayT::Int(LooseIntCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<IntCallbackT>(Callback); }
void ReadArrayT::UInt(LooseUIntCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<UIntCallbackT>(Callback); }
void ReadArrayT::Float(LooseFloatCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<FloatCallbackT>(Callback); }
void ReadArrayT::String(LooseStringCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<StringCallbackT>(Callback); }
void ReadArrayT::Binary(LooseBinaryCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<BinaryCallbackT>(Callback); }
void ReadArrayT::Object(LooseObjectCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<ObjectCallbackT>(Callback); }
void ReadArrayT::Array(LooseArrayCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<ArrayCallbackT>(Callback); }
void ReadArrayT::Polymorph(LoosePolymorphCallbackT const &Callback) { Assert(!this->Callback); this->Callback.Set<PolymorphCallbackT>(Callback); }

void ReadArrayT::Finally(std::function<ReadErrorT(void)> const &Callback) { Assert(!DestructorCallback); DestructorCallback = Callback; }

void ReadArrayT::InternalPolymorph(InternalPolymorphCallbackT const &Callback) { Assert(!this->Callback); this->Callback = Callback; }

ReadErrorT ReadArrayT::Bool(bool Value) 
{ 
	if (Callback.Is<BoolCallbackT>()) return Callback.Get<BoolCallbackT>()(Value);
	else return std::string("Bool element found in array that does not have a bool handler.");
}

ReadErrorT ReadArrayT::Number(std::string const &Source)
{
	return ReadNumber(Callback, Source, true);
}

ReadErrorT ReadArrayT::StringOrBinary(std::string const &Source)
{
	return ReadString(Callback, Source, true);
}

ReadErrorT ReadArrayT::Object(ReadObjectT &Object)
{ 
	if (Callback.Is<ObjectCallbackT>()) return Callback.Get<ObjectCallbackT>()(std::ref(Object)); 
	else if (Callback.Is<InternalPolymorphCallbackT>())
		return Callback.Get<InternalPolymorphCallbackT>().ObjectCallback(std::ref(Object));
	else return std::string("Object element found in array that does not have an object handler.");
}

ReadErrorT ReadArrayT::Key(std::string const &Value)
	{ return std::string("Keys may not appear in arrays."); } // Hopefully yajl will catch this first?
	
ReadErrorT ReadArrayT::Array(ReadArrayT &Array)
{ 
	if (Callback.Is<ArrayCallbackT>()) return Callback.Get<ArrayCallbackT>()(std::ref(Array));
	else if (Callback.Is<PolymorphCallbackT>()) return ReadPolymorph(Callback, Array, true);
	else return std::string("Array element found in array that does not have an array handler.");
}

ReadErrorT ReadArrayT::Final(void)
{
	if (DestructorCallback) return DestructorCallback();
	return {};
}

//----------------------------------------------------------------------------------------------------------------
// Nested object reader
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
void ReadObjectT::Polymorph(std::string const &Key, LoosePolymorphCallbackT const &Callback) 
	{ Assert(!Callbacks[Key]); Callbacks[Key].Set<PolymorphCallbackT>(Callback); }

void ReadObjectT::Finally(std::function<ReadErrorT(void)> const &Callback) { Assert(!DestructorCallback); DestructorCallback = Callback; }

ReadErrorT ReadObjectT::Bool(bool Value) 
{ 
	if (LastKey.empty()) { return std::string("Value with no key in object."); }
	auto Callback = Callbacks.find(LastKey);
	if (Callback != Callbacks.end())
		return Callback->second.Get<BoolCallbackT>()(Value);
	LastKey.clear();
	return {};
}
	
ReadErrorT ReadObjectT::Number(std::string const &Source)
{ 
	if (LastKey.empty()) { return std::string("Value with no key in object."); }
	auto Callback = Callbacks.find(LastKey);
	LastKey.clear();
	if (Callback == Callbacks.end()) return {};
	return ReadNumber(Callback->second, Source);
}

ReadErrorT ReadObjectT::StringOrBinary(std::string const &Source)
{ 
	if (LastKey.empty()) { return std::string("Value with no key in object."); }
	auto Callback = Callbacks.find(LastKey);
	LastKey.clear();
	if (Callback == Callbacks.end()) return {};
	return ReadString(Callback->second, Source);
}

ReadErrorT ReadObjectT::Object(ReadObjectT &Object)
{ 
	if (LastKey.empty()) { return std::string("Value with no key in object."); }
	auto Callback = Callbacks.find(LastKey);
	if ((Callback != Callbacks.end()) && Callback->second.Is<ObjectCallbackT>())
		return Callback->second.Get<ObjectCallbackT>()(std::ref(Object));
	LastKey.clear();
	return {};
}

ReadErrorT ReadObjectT::Key(std::string const &Value) 
{ 
	LastKey = Value; 
	return {}; 
}

ReadErrorT ReadObjectT::Array(ReadArrayT &Array)
{
	if (LastKey.empty()) { return std::string("Value with no key in object."); }
	auto Callback = Callbacks.find(LastKey);
	if (Callback != Callbacks.end())
	{
		if (Callback->second.Is<ArrayCallbackT>())
		{
			return Callback->second.Get<ArrayCallbackT>()(std::ref(Array));
		}
		else if (Callback->second.Is<PolymorphCallbackT>())
		{
			return ReadPolymorph(Callback->second, Array);
		}
	}
	LastKey.clear();
	return {};
}

ReadErrorT ReadObjectT::Final(void)
{
	if (DestructorCallback) return DestructorCallback();
	return {};
}

//----------------------------------------------------------------------------------------------------------------
// Nested object reader
ReadT::ReadT(void)
{
	Stack.emplace(this, [](ReadNestableT *){});
	
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
			Assert(This);
			auto Error = This->Stack.top()->Bool(Value);
			if (Error) { This->Error = *Error; return false; }
			return true;
		},
		nullptr,
		nullptr,
		[](void *UserData, char const *Value, size_t ValueLength) -> int // Number
		{
			auto This = PrepareUserData(UserData); 
			Assert(This);
			auto Error = This->Stack.top()->Number(std::string(Value, ValueLength));
			if (Error) { This->Error = *Error; return false; }
			return true;
		},
		[](void *UserData, unsigned char const *Value, size_t ValueLength) -> int // String/Binary
		{
			auto This = PrepareUserData(UserData); 
			Assert(This);
			auto Error = This->Stack.top()->StringOrBinary(std::string(reinterpret_cast<char const *>(Value), ValueLength));
			if (Error) { This->Error = *Error; return false; }
			return true;
		},
		
		// Object
		[](void *UserData) -> int // Open
		{
			auto This = PrepareUserData(UserData); 
			Assert(This);
			auto NewTop = new ReadObjectT;
			//if (!This->Object(*NewTop)) return false;
			auto Error = This->Stack.top()->Object(*NewTop);
			if (Error) { This->Error = *Error; return false; }
			This->Stack.emplace(NewTop, [](ReadNestableT *Target) { delete Target; });
			return true;
		},
		[](void *UserData, unsigned char const *Key, size_t KeyLength) -> int // Key
		{
			auto This = PrepareUserData(UserData); 
			Assert(This);
			auto Error = This->Stack.top()->Key(std::string(reinterpret_cast<char const *>(Key), KeyLength));
			if (Error) { This->Error = *Error; return false; }
			return true;
		},
		[](void *UserData) -> int // Close
		{
			auto This = PrepareUserData(UserData); 
			Assert(This);
			This->Stack.top()->Final(); This->Stack.pop();
			return true;
		},
		
		// Array
		[](void *UserData) -> int // Open
		{
			auto This = PrepareUserData(UserData); 
			Assert(This);
			auto NewTop = new ReadArrayT;
			//if (!This->Array(*NewTop)) return false;
			auto Error = This->Stack.top()->Array(*NewTop);
			if (Error) { This->Error = *Error; return false; }
			This->Stack.emplace(NewTop, [](ReadNestableT *Target) { delete Target; });
			return true;
		},
		[](void *UserData) -> int // Close
		{
			auto This = PrepareUserData(UserData); 
			Assert(This);
			This->Stack.top()->Final(); This->Stack.pop();
			return true;
		}
	};
	Base = yajl_alloc(&Callbacks, NULL, this);  
}

ReadT::~ReadT(void)
{
	yajl_free(Base);
}
		
ReadErrorT ReadT::Parse(Filesystem::PathT const &Path)
{
	auto File = fopen(Path->Render().c_str(), "r");
	if (!File || ferror(File)) return (::StringT() << "Unable to open file " << Path->Render() << " to parse.").str();
	while (true)
	{
		uint8_t ReadBuffer[65536];
		auto ReadSize = fread(reinterpret_cast<char *>(ReadBuffer), 1, sizeof(ReadBuffer) - 1, File);
		std::cout << "read [" << std::string((char const *)ReadBuffer, ReadSize) << "] " << ReadSize << std::endl;
		if (ReadSize == 0)
		{
			auto Result = yajl_complete_parse(Base);
			if (Result != yajl_status_ok) goto ReadError;
			break;
		}
		ReadBuffer[ReadSize] = '\0';
		{
			auto Result = yajl_parse(Base, ReadBuffer, ReadSize);
			if (Result != yajl_status_ok) goto ReadError;
		}
		
		continue;
		ReadError:
		{
			auto ErrorMessage = yajl_get_error(Base, 1, ReadBuffer, ReadSize);
			::StringT Error;
			Error << "Error during JSON deserialization of " << Path->Render() << ": ";
			if (this->Error) Error << *this->Error << "\n";
			Error << ErrorMessage;
			yajl_free_error(Base, ErrorMessage);
			return Error.str();
		}
	}
	return {};
}

ReadErrorT ReadT::Parse(std::istream &Stream)
{
	Error.Unset();
	while (true)
	{
		uint8_t ReadBuffer[65536];
		Stream.read(reinterpret_cast<char *>(ReadBuffer), sizeof(ReadBuffer) - 1);
		auto ReadSize = Stream.gcount();
		if (ReadSize == 0)
		{
			auto Result = yajl_complete_parse(Base);
			if (Result != yajl_status_ok) goto ReadError;
			break;
		}
		ReadBuffer[ReadSize] = '\0';
		{
			auto Result = yajl_parse(Base, ReadBuffer, ReadSize);
			if (Result != yajl_status_ok) goto ReadError;
		}
		
		continue;
		ReadError:
		{
			auto ErrorMessage = yajl_get_error(Base, 1, ReadBuffer, ReadSize);
			::StringT Error;
			Error << "Error during JSON deserialization: ";
			if (this->Error) Error << *this->Error << "\n";
			Error << ErrorMessage;
			yajl_free_error(Base, ErrorMessage);
			return Error.str();
		}
	}
	return {};
}

ReadErrorT ReadT::Parse(std::istream &&Stream) { return Parse(Stream); }

}
