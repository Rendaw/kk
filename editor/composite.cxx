#include "composite.h"

#include "protoatom.h"

namespace Core
{

CompositeT::CompositeT(CoreT &Core, CompositeTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo) 
{
	Visual.Tag().Add(TypeInfo.Tag);
}

void CompositeT::Serialize(Serial::WritePolymorphT &Polymorph) const
{
	for (auto &Part : Parts)
		Part->Serialize(Polymorph);
}

AtomTypeT const &CompositeT::GetTypeInfo(void) const
{
	return TypeInfo;
}

void CompositeT::Focus(FocusDirectionT Direction)
{
	switch (Direction)
	{
		case FocusDirectionT::FromAhead:
			Focused = PartFocusedT{Parts.size() - 1};
			Parts.back()->Focus(Direction);
			return;
		case FocusDirectionT::FromBehind:
			Focused = PartFocusedT{0};
			Parts.front()->Focus(Direction);
			return;
		case FocusDirectionT::Direct:
			Focused = SelfFocusedT{};
			NucleusT::Focus(Direction);
			Visual.SetClass("flag-focused");
			return;
	}
}

void CompositeT::Defocus(void)
{
	Visual.UnsetClass("flag-focused");
}

void CompositeT::AssumeFocus(void)
{
	if (Focused)
	{
		// If self focused, do nothing
		if (Focused.Is<PartFocusedT>()) Parts[Focused.Get<PartFocusedT>()]->AssumeFocus();
	}
	else
	{
		size_t Index = 0;
		for (auto &PartInfo : TypeInfo.Parts)
		{
			if (PartInfo->FocusDefault) 
			{
				Focused = PartFocusedT{Index};
				Parts[Index]->AssumeFocus();
				return;
			}
			++Index;
		}
		Focus(FocusDirectionT::Direct);
	}
}

void CompositeT::Refresh(void)
{
	Visual.Start();
	if (TypeInfo.DisplayPrefix) Visual.Add(*TypeInfo.DisplayPrefix);
	size_t Index = 0;
	for (auto &Part : Parts)
	{
		if ((Index == 1) && TypeInfo.DisplayInfix)
			Visual.Add(*TypeInfo.DisplayInfix);
		Visual.Add(Part->Visual);
		++Index;
	}
	if (TypeInfo.DisplaySuffix) Visual.Add(*TypeInfo.DisplaySuffix);
}

std::unique_ptr<ActionT> CompositeT::Set(NucleusT *Nucleus)
{
	size_t Index = 0;
	for (auto &PartInfo : TypeInfo.Parts)
	{
		if (PartInfo->SetDefault) 
			return Parts[Index]->Set(Nucleus);
		++Index;
	}
	Assert(false);
	return {};
}

std::unique_ptr<ActionT> CompositeT::Set(std::string const &Text)	
{
	size_t Index = 0;
	for (auto &PartInfo : TypeInfo.Parts)
	{
		if (PartInfo->SetDefault) 
			return Parts[Index]->Set(Text);
		Index += 1;
	}
	Assert(false);
	return {};
}

OptionalT<std::unique_ptr<ActionT>> CompositeT::HandleInput(InputT const &Input)
{
	if (Input.Main)
	{
		if (Focused.Is<PartFocusedT>())
		{
			if (TypeInfo.SpatiallyVertical)
			{
				switch (*Input.Main)
				{
					case InputT::MainT::Up: FocusPrevious(); return {};
					case InputT::MainT::Down: FocusNext(); return {};
					default: break;
				}
			}
			else
			{
				switch (*Input.Main)
				{
					case InputT::MainT::Left: FocusPrevious(); return {};
					case InputT::MainT::Right: FocusNext(); return {};
					default: break;
				}
			}
		}
		if (Focused.Is<SelfFocusedT>())
		{
			if (Atom) return std::unique_ptr<ActionT>(new AtomT::SetT(*Atom, new ProtoatomT(Core)));
		}
	}
	if (Parent) return Parent->HandleInput(Input);
	return {};
}

void CompositeT::FocusPrevious(void)
{
	Assert(Focused.Is<PartFocusedT>());
	auto &Index = Focused.Get<PartFocusedT>();
	if (Index == 0) { if (Parent) Parent->FocusPrevious(); }
	else 
	{
		Index -= 1;
		Parts[Index]->Focus(FocusDirectionT::FromAhead);
	}
}

void CompositeT::FocusNext(void)
{
	Assert(Focused.Is<PartFocusedT>());
	auto &Index = Focused.Get<PartFocusedT>();
	if (Index + 1 == Parts.size()) { if (Parent) Parent->FocusNext(); }
	else 
	{
		Index += 1;
		Parts[Index]->Focus(FocusDirectionT::FromBehind);
	}
}

NucleusT *CompositeTypeT::Generate(CoreT &Core)
{
	auto Out = new CompositeT(Core, *this);
	for (auto &Part : Parts) 
	{
		Out->Parts.emplace_back(Core);
		Out->Parts.back().Parent = Out;
		Out->Parts.back().Set(Part->Generate(Core));
	}
	return Out;
}

CompositeTypePartT::CompositeTypePartT(CompositeTypeT &Parent) : Parent(Parent) {}

NucleusT *AtomPartTypeT::Generate(CoreT &Core)
{
	return new AtomPartT(Core, *this);
}

AtomPartT::AtomPartT(CoreT &Core, AtomPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), Data(Core)
{
	Data.Callback = [this](AtomT &Changed) { FlagRefresh(); };
	Data.Set(new ProtoatomT(Core));
}

void AtomPartT::Parented(void)
{
	Data.Parent = Parent.Nucleus;
	if (Data) Data.Nucleus->Parent.Set(Parent.Nucleus);
}

void AtomPartT::Serialize(Serial::WritePolymorphT &Polymorph) const
	{ Data->Serialize(Polymorph.Polymorph(TypeInfo.Tag)); }

AtomTypeT const &AtomPartT::GetTypeInfo(void) const { return TypeInfo; }

void AtomPartT::Focus(FocusDirectionT Direction) { Data->Focus(Direction); }

void AtomPartT::Defocus(void) {}

void AtomPartT::AssumeFocus(void) { Data->AssumeFocus(); }

void AtomPartT::Refresh(void) 
{
	Visual.Start();
	Visual.Add(Data->Visual);
}

std::unique_ptr<ActionT> AtomPartT::Set(NucleusT *Nucleus) { return Data->Set(Nucleus); }

std::unique_ptr<ActionT> AtomPartT::Set(std::string const &Text) { return Data->Set(Text); }

OptionalT<std::unique_ptr<ActionT>> AtomPartT::HandleInput(InputT const &Input) { return Data->HandleInput(Input); }

void AtomPartT::FocusPrevious(void) { Parent->FocusPrevious(); }

void AtomPartT::FocusNext(void) { Parent->FocusNext(); }

NucleusT *AtomListPartTypeT::Generate(CoreT &Core)
{
	return new AtomListPartT(Core, *this);
}

AtomListPartT::AtomListPartT(CoreT &Core, AtomListPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), FocusIndex(0)
{
	Add(0, new ProtoatomT(Core));
}

void AtomListPartT::Parented(void)
{
	for (auto &Atom : Data)
	{
		Atom->Atom.Parent = Parent.Nucleus;
		if (Atom) Atom->Atom.Nucleus->Parent.Set(Parent.Nucleus);
	}
}

void AtomListPartT::Serialize(Serial::WritePolymorphT &Polymorph) const
{
	auto Array = Polymorph.Array(TypeInfo.Tag);
	for (auto &Atom : Data)
		{ Atom->Atom->Serialize(Array.Polymorph()); }
}

AtomTypeT const &AtomListPartT::GetTypeInfo(void) const { return TypeInfo; }

void AtomListPartT::Focus(FocusDirectionT Direction) 
{ 
	switch (Direction)
	{
		case FocusDirectionT::FromAhead: FocusIndex = Data.size() - 1; break;
		case FocusDirectionT::FromBehind: FocusIndex = 0; break;
		case FocusDirectionT::Direct: 
			if (FocusIndex + 1 >= Data.size()) FocusIndex = Data.size() - 1;
			break;
	}
	Data[FocusIndex]->Atom->Focus(Direction);
}

void AtomListPartT::Defocus(void) { Assert(false); }

void AtomListPartT::AssumeFocus(void) 
{
	if (FocusIndex + 1 >= Data.size()) FocusIndex = Data.size() - 1;
	Data[FocusIndex]->Atom->AssumeFocus();
}

void AtomListPartT::Refresh(void) 
{
	Visual.Start();
	for (auto &Atom : Data)
	{
		Atom->Visual.Start();
		Atom->Visual.Add(Atom->Atom->Visual);
		Visual.Add(Atom->Visual);
	}
}

void AtomListPartT::Add(size_t Position, NucleusT *Nucleus)
{
	Data.emplace(Data.begin() + Position, new ItemT{{Core.RootVisual.Root}, {Core}});
	Data[Position]->Atom.Parent = Parent.Nucleus;
	Data[Position]->Atom.Callback = [this](AtomT &This)
	{
		FlagRefresh();
	};
	Data[Position]->Atom.Set(Nucleus);
}

void AtomListPartT::Remove(size_t Position)
{
	Assert(Position < Data.size());
	Data.erase(Data.begin() + Position);
}


AtomListPartT::AddRemoveT::AddRemoveT(AtomListPartT &Base, bool Add, size_t Position, NucleusT *Nucleus) : Base(Base), Add(Add), Position(Position), Nucleus(Base.Core, Nucleus) { }

std::unique_ptr<ActionT> AtomListPartT::AddRemoveT::Apply(void)
{
	std::unique_ptr<ActionT> Out;
	if (Add) 
	{
		Out.reset(new AddRemoveT(Base, false, Position, nullptr));
		Base.Add(Position, Nucleus.Nucleus);
	}
	else 
	{
		Out.reset(new AddRemoveT(Base, true, Position, Base.Data[Position]->Atom.Nucleus));
		Base.Remove(Position);
	}
	return std::move(Out);
}

std::unique_ptr<ActionT> AtomListPartT::Set(NucleusT *Nucleus) 
{
	return std::unique_ptr<ActionT>(new AddRemoveT(*this, true, Data.size(), Nucleus));
}

std::unique_ptr<ActionT> AtomListPartT::Set(std::string const &Text) { Assert(false); return {}; }

OptionalT<std::unique_ptr<ActionT>> AtomListPartT::HandleInput(InputT const &Input) 
{ 
	if (Input.Main)
	{
		if (TypeInfo.Parent.SpatiallyVertical)
		{
			switch (*Input.Main)
			{
				case InputT::MainT::Up: FocusPrevious(); return {};
				case InputT::MainT::Down: FocusNext(); return {};
				default: break;
			}
		}
		else
		{
			switch (*Input.Main)
			{
				case InputT::MainT::Left: FocusPrevious(); return {};
				case InputT::MainT::Right: FocusNext(); return {};
				default: break;
			}
		}

		switch (*Input.Main)
		{
			case InputT::MainT::NewStatement:
				return std::unique_ptr<ActionT>(new AddRemoveT(*this, true, FocusIndex, new ProtoatomT(Core)));
			case InputT::MainT::Delete:
				return std::unique_ptr<ActionT>(new AddRemoveT(*this, false, FocusIndex, {}));
			default: break;
		}
	}
	return Parent->HandleInput(Input);
}

void AtomListPartT::FocusPrevious(void) 
{ 
	if (FocusIndex == 0) Parent->FocusPrevious();
	else 
	{
		FocusIndex -= 1;
		Data[FocusIndex]->Atom->Focus(FocusDirectionT::FromAhead);
	}
}

void AtomListPartT::FocusNext(void) 
{ 
	if (FocusIndex + 1 == Data.size()) Parent->FocusNext();
	else 
	{
		FocusIndex += 1;
		Data[FocusIndex]->Atom->Focus(FocusDirectionT::FromBehind);
	}
}

NucleusT *StringPartTypeT::Generate(CoreT &Core)
{
	return new StringPartT(Core, *this);
}

StringPartT::StringPartT(CoreT &Core, StringPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo)
{
}

void StringPartT::Serialize(Serial::WritePolymorphT &Polymorph) const
	{ Polymorph.String(TypeInfo.Tag, Data); }

AtomTypeT const &StringPartT::GetTypeInfo(void) const { return TypeInfo; }

void StringPartT::Focus(FocusDirectionT Direction) 
{ 
	NucleusT::Focus(Direction);
	Focused = true;
	switch (Direction)
	{
		case FocusDirectionT::FromAhead: Position = Data.size(); break;
		case FocusDirectionT::FromBehind: Position = 0; break;
		case FocusDirectionT::Direct: Position = 0; break;
	}
	Visual.SetClass("flag-focused");
	FlagRefresh();
}

void StringPartT::Defocus(void) 
{
	Focused = false;
	Visual.UnsetClass("flag-focused");
	FlagRefresh();
}

void StringPartT::AssumeFocus(void) 
{ 
	Focus(FocusDirectionT::Direct);
}

void StringPartT::Refresh(void) 
{
	Visual.Start();
	if (TypeInfo.DisplayPrefix) Visual.Add(*TypeInfo.DisplayPrefix);
	if (Focused)
	{
		Visual.Add(Data.substr(0, Position));
		Visual.Add(Core.CursorVisual);
		Visual.Add(Data.substr(Position));
	}
	else Visual.Add(Data);
	if (TypeInfo.DisplaySuffix) Visual.Add(*TypeInfo.DisplaySuffix);
}

std::unique_ptr<ActionT> StringPartT::Set(NucleusT *Nucleus) { Assert(false); return {}; }

StringPartT::SetT::SetT(StringPartT &Base, unsigned int Position, std::string const &Data) : Base(Base), Position(Position), Data(Data) {}

std::unique_ptr<ActionT> StringPartT::SetT::Apply(void)
{
	auto Out = new SetT(Base, Base.Position, Base.Data);
	Base.Data = Data;
	Base.Position = Position;
	Base.FlagRefresh();
	return std::unique_ptr<ActionT>(Out);
}

bool StringPartT::SetT::Combine(std::unique_ptr<ActionT> &Other)
{
	auto Set = dynamic_cast<SetT *>(Other.get());
	if (!Set) return false;
	if (&Set->Base != &Base) return false;
	Position = Set->Position;
	Data = Set->Data;
	return true;
}

std::unique_ptr<ActionT> StringPartT::Set(std::string const &Text) 
{ 
	return std::unique_ptr<ActionT>(new SetT(*this, Position, Text));
}

OptionalT<std::unique_ptr<ActionT>> StringPartT::HandleInput(InputT const &Input) 
{ 
	if (Input.Text)
	{
		return std::unique_ptr<ActionT>(new SetT(
			*this, 
			Position + 1, 
			Data.substr(0, Position) + *Input.Text + Data.substr(Position)));
	}
	else if (Input.Main)
	{
		switch (*Input.Main)
		{
			case InputT::MainT::Left:
			{
				if (Position == 0) 
				{
					Parent->FocusPrevious();
				}
				else
				{
					Position -= 1;
					FlagRefresh();
				}
				return {};
			}
			case InputT::MainT::Right:
			{
				if (Position == Data.size()) 
				{
					Parent->FocusNext();
				}
				else
				{
					Position += 1;
					FlagRefresh();
				}
				return {};
			}
			case InputT::MainT::TextBackspace:
			{
				if (Position == 0) return {};
				Data.erase(Position - 1, 1);
				Position -= 1;
				FlagRefresh();
				return {};
			}
			case InputT::MainT::Delete:
			{
				if (Position == Data.size()) return {};
				Data.erase(Position, 1);
				FlagRefresh();
				return {};
			}
			default: break;
		}
	}
	return Parent->HandleInput(Input);
}

NucleusT *EnumPartTypeT::Generate(CoreT &Core)
{
	return new EnumPartT(Core, *this);
}

EnumPartT::EnumPartT(CoreT &Core, EnumPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), Index(0) {}

void EnumPartT::Serialize(Serial::WritePolymorphT &Polymorph) const
{ 
	if (Index >= TypeInfo.Values.size())
	{
		Assert(false);
		Polymorph.String(TypeInfo.Tag, "Invalid");
	}
	else
	{
		Polymorph.String(TypeInfo.Tag, TypeInfo.Values[Index]);
	}
}

AtomTypeT const &EnumPartT::GetTypeInfo(void) const { return TypeInfo; }

void EnumPartT::Focus(FocusDirectionT Direction)
{ 
	NucleusT::Focus(Direction);
	Visual.SetClass("flag-focused");
}

void EnumPartT::Defocus(void)
{
	Visual.UnsetClass("flag-focused");
}

void EnumPartT::AssumeFocus(void)
{ 
	Focus(FocusDirectionT::Direct);
}

void EnumPartT::Refresh(void)
{ 
	Visual.Start();
	if (Index >= TypeInfo.Values.size())
	{
		Assert(false);
		Visual.Add("Invalid");
	}
	else
	{
		Visual.Add(TypeInfo.Values[Index]);
	}
}
	
EnumPartT::SetT::SetT(EnumPartT &Base, size_t Index) : Base(Base), Index(Index)
{
}

std::unique_ptr<ActionT> EnumPartT::SetT::Apply(void)
{
	auto Out = new SetT(Base, Index);
	Base.Index = Index;
	Base.FlagRefresh();
	return std::unique_ptr<ActionT>(Out);
}
	
std::unique_ptr<ActionT> EnumPartT::Set(NucleusT *Nucleus) { Assert(false); return {}; }

std::unique_ptr<ActionT> EnumPartT::Set(std::string const &Text) { Assert(false); return {}; }

OptionalT<std::unique_ptr<ActionT>> EnumPartT::HandleInput(InputT const &Input)
{ 
	if (Input.Main)
	{
		switch (*Input.Main)
		{
			case InputT::MainT::Enter: 
			{
				return std::unique_ptr<ActionT>(new SetT(*this, (Index + 1) % TypeInfo.Values.size()));
			}
			default: break;
		}
	}
	return Parent->HandleInput(Input);
}

}
