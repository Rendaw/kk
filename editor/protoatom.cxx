#include "protoatom.h"

#include "../shared/regex.h"

#include "composite.h"

namespace Core
{

NucleusT *HoldPartTypeT::Generate(CoreT &Core)
{
	return new HoldPartT(Core, *this);
}

HoldPartT::HoldPartT(CoreT &Core, HoldPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo), PrefixVisual(Core.RootVisual.Root), SuffixVisual(Core.RootVisual.Root), Data(Core)
{
	Visual.SetClass("part");
	Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
	PrefixVisual.SetClass("affix-inner");
	SuffixVisual.SetClass("affix-inner");
}

void HoldPartT::Serialize(Serial::WritePolymorphT &Polymorph) const
{
	if (!Data) return;
	Data->Serialize(Polymorph.Polymorph(TypeInfo.Tag)); 
}

AtomTypeT const &HoldPartT::GetTypeInfo(void) const { return TypeInfo; }

void HoldPartT::Focus(FocusDirectionT Direction) 
{ 
	if (Data) Data->Focus(Direction); 
	else
	{
		switch (Direction)
		{
			case FocusDirectionT::FromBehind:
			case FocusDirectionT::Direct: 
				Parent->FocusNext();
				return;
			case FocusDirectionT::FromAhead: 
				Parent->FocusPrevious();
				return;
		}
	}
}

void HoldPartT::Defocus(void) {}

void HoldPartT::AssumeFocus(void) 
{ 
	if (Data) Data->AssumeFocus(); 
	else Assert(false);
}

void HoldPartT::Refresh(void) 
{
	if (TypeInfo.DisplayPrefix) 
	{
		PrefixVisual.Start();
		PrefixVisual.Add(*TypeInfo.DisplayPrefix);
	}
	if (TypeInfo.DisplaySuffix) 
	{
		SuffixVisual.Start();
		SuffixVisual.Add(*TypeInfo.DisplaySuffix);
	}
	
	Visual.Start();
	if (Data)
	{
		auto Protoatom = AsProtoatom(Data);
		Assert(!Protoatom);
	}
	Visual.Add(PrefixVisual);
	if (Data) Visual.Add(Data->Visual);
	Visual.Add(SuffixVisual);
}

void HoldPartT::SetHold(NucleusT *Nucleus) { Data.Set(Nucleus); }

OptionalT<std::unique_ptr<ActionT>> HoldPartT::HandleInput(InputT const &Input) { return Data->HandleInput(Input); }

void HoldPartT::FocusPrevious(void) { Parent->FocusPrevious(); }

void HoldPartT::FocusNext(void) { Parent->FocusNext(); }

ProtoatomTypeT::ProtoatomTypeT(void)
{
	Tag = "Protoatom";
	{
		auto Part = new HoldPartTypeT(*this);
		Part->Tag = "Lifted";
		//Part->SetDefault = true; // Unnecessary, since protoatom is only added explictly (no generic bubling sets or whatnot)
		Parts.emplace_back(Part);
	}
	{
		auto Part = new ProtoatomPartTypeT(*this);
		Part->Tag = "Data";
		Part->FocusDefault = true;
		Parts.emplace_back(Part);
	}
}

NucleusT *ProtoatomPartTypeT::Generate(CoreT &Core)
{
	return new ProtoatomPartT(Core, *this);
}

ProtoatomPartT::ProtoatomPartT(CoreT &Core, ProtoatomPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo)
{
	Visual.SetClass("part");
	Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
}

void ProtoatomPartT::Serialize(Serial::WritePolymorphT &WritePolymorph) const
{
	if (IsIdentifier) WritePolymorph.Bool("IsIdentifier", *IsIdentifier);
	WritePolymorph.String("Data", Data);
	WritePolymorph.UInt("Position", Position);
}

AtomTypeT const &ProtoatomPartT::GetTypeInfo(void) const { return TypeInfo; }
	
void ProtoatomPartT::Focus(FocusDirectionT Direction)
{
	if ((Direction == FocusDirectionT::FromBehind) ||
		(Direction == FocusDirectionT::Direct))
	{
		Position = 0;
	}
	else if (Direction == FocusDirectionT::FromAhead)
	{
		Position = Data.size();
	}
	Focused = true;
	Visual.SetClass("flag-focused");
	FlagRefresh();
	NucleusT::Focus(Direction);
	for (auto &Atom : FocusDependents) Atom->FlagRefresh();
}

void ProtoatomPartT::Defocus(void) 
{
	Focused = false;
	Visual.UnsetClass("flag-focused");
	FlagRefresh();
	for (auto &Atom : FocusDependents) Atom->FlagRefresh();
}

void ProtoatomPartT::AssumeFocus(void)
{
	Focus(FocusDirectionT::Direct);
}

void ProtoatomPartT::Refresh(void)
{
	Visual.Start();
	if (Focused)
	{
		Visual.Add(Data.substr(0, Position));
		Visual.Add(Core.CursorVisual);
		Visual.Add(Data.substr(Position));
	}
	else Visual.Add(Data);
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomPartT::HandleInput(InputT const &Input)
{
	if (Input.Text)
	{
		auto Text = *Input.Text;

		static auto IdentifierClass = Regex::ParserT<>("[a-zA-Z0-9_]");
		auto NewIsIdentifier = IdentifierClass(Text);
		
		if (Text == " ") return Finish({}, {}, {});
		else if (Text == "\n") return Finish({}, {}, InputT{InputT::MainT::NewStatement, {}});
		else if (Text == ";") return Finish({}, {}, InputT{InputT::MainT::NewStatement, {}});
		else if (!IsIdentifier || (*IsIdentifier == NewIsIdentifier))
		{
			if (!IsIdentifier) IsIdentifier = NewIsIdentifier;
			auto NewData = Data;
			NewData.insert(Position, Text);
			auto NewPosition = Position + 1;
			if (NewPosition == NewData.size()) // Only do auto conversion if appending text
			{
				auto Found = Core.LookUpAtom(NewData);
				if (Found && Found->ReplaceImmediately) 
					return Finish(Found, NewData, {});
			}

			struct SetT : ActionT
			{
				ProtoatomPartT &Base;
				unsigned int Position;
				std::string Data;
				
				SetT(ProtoatomPartT &Base, unsigned int Position, std::string const &Data) : Base(Base), Position(Position), Data(Data) {}
				
				std::unique_ptr<ActionT> Apply(void)
				{
					auto Out = new SetT(Base, Base.Position, Base.Data);
					Base.Data = Data;
					Base.Position = Position;
					Base.FlagRefresh();
					return std::unique_ptr<ActionT>(Out);
				}
				
				bool Combine(std::unique_ptr<ActionT> &Other) override
				{
					auto Set = dynamic_cast<SetT *>(Other.get());
					if (!Set) return false;
					if (&Set->Base != &Base) return false;
					Position = Set->Position;
					Data = Set->Data;
					return true;
				}
			};
			return std::unique_ptr<ActionT>(new SetT(*this, NewPosition, Data + Text));
		}
		else if (IsIdentifier && (*IsIdentifier != NewIsIdentifier))
		{
			return Finish({}, {}, {Input});
		}
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
			} return {};
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
			} return {};
			case InputT::MainT::TextBackspace:
			{
				if (Position == 0) return {};
				Data.erase(Position - 1, 1);
				Position -= 1;
				FlagRefresh();
			} return {};
			case InputT::MainT::Delete:
			{
				if (Position == Data.size()) return {};
				Data.erase(Position, 1);
				FlagRefresh();
			} return {};
			default: break;
		}
	}
	
	// TODO
	// The issue is that AtomListPartT sets Parent to Composite, so Parent->HandleInput never goes to the AtomListPart
	if (Parent)
	{
		auto Result = Parent->HandleInput(Input);
		if (Result)
		{
			auto ActionGroup = new ActionGroupT;
			auto FinishResult = Finish({}, {}, {});
			if (FinishResult) ActionGroup->Add(std::move(*FinishResult));
			ActionGroup->Add(std::move(*Result));
			return std::unique_ptr<ActionT>(ActionGroup);
		}
	}

	return {};
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomPartT::Finish(
	OptionalT<AtomTypeT *> Type, 
	OptionalT<std::string> NewData, 
	OptionalT<InputT> SeedData)
{
	Assert(Parent->Atom);
	auto &Lifted = Parent->As<CompositeT>()->Parts[0]->As<HoldPartT>()->Data;
	std::string Text = NewData ? *NewData : Data;
	Type = Type ? Type : Core.LookUpAtom(Data);
	auto Actions = new ActionGroupT;
	if (Data.empty())
	{
		if (SeedData) Actions->Add(Core.ActionHandleInput(*SeedData));
	}
	else if (Type)
	{
		Assert(!SeedData);
		if (((*Type)->Arity == ArityT::Nullary) || (*Type)->Prefix)
		{
			if (Lifted)
			{
				std::cout << "NOTE: Can't finish to nullary or prefixed op if protoatom has lifed." << std::endl; 
				return {}; 
			}
			auto Replacement = Core.ProtoatomType->Generate(Core);
			auto Finished = Type->Generate(Core);
			Actions->Add(Replacement->Set(Finished));
			Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*Parent->Atom, Replacement)));
		}
		else
		{
			auto Child = Lifted ? Lifted.Nucleus : Core.ProtoatomType->Generate(Core);
			Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*Parent->Atom, Child)));
			AtomT *WedgeAtom = Parent->Atom;
			while (WedgeAtom->Parent && 
				(
					(WedgeAtom->Parent->GetTypeInfo().Precedence > (*Type)->Precedence) || 
					(
						(WedgeAtom->Parent->GetTypeInfo().Precedence == (*Type)->Precedence) &&
						(WedgeAtom->Parent->GetTypeInfo().LeftAssociative)
					)
				))
			{
				WedgeAtom = WedgeAtom->Parent->Atom;
				Child = WedgeAtom->Nucleus;
			}
			auto Finished = (*Type)->Generate(Core);
			Assert(Finished);
			Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*WedgeAtom, Finished)));
			Actions->Add(Finished->Set(Child));
		}
	}
	else
	{
		if (Lifted) 
		{
			if (Data.empty())
			{
				Assert(Atom);
				std::cout << "Replacing " << this << " with " << Lifted.Nucleus << ", atom " << Atom << std::endl;
				return std::unique_ptr<ActionT>(new AtomT::SetT(*Parent->Atom, Lifted.Nucleus));
			}

			std::cout << "NOTE: Can't finish as new element if protoatom has lifted." << std::endl; 
			return {};
		}
		
		auto String = Core.StringType->Generate(Core);
		Actions->Add(String->Set(Text));

		auto Protoatom = Core.ProtoatomType->Generate(Core);

		auto ParentAsComposite = Parent->As<CompositeT>();
		if (ParentAsComposite && (&ParentAsComposite->TypeInfo == Core.ElementType.get()))
		{
			Protoatom->As<CompositeT>()->Parts[0]->As<HoldPartT>()->SetHold(String);
		}
		else
		{
			auto Element = Core.ElementType->Generate(Core);
			Actions->Add(Element->As<CompositeT>()->Parts[1]->Set(String));
			Protoatom->As<CompositeT>()->Parts[0]->As<HoldPartT>()->SetHold(Element);
		}

		std::cout << Core.Dump() << std::endl; // DEBUG
		Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*Parent->Atom, Protoatom)));

		if (SeedData) Actions->Add(Core.ActionHandleInput(*SeedData));
		std::cout << "Standard no-type finish." << std::endl;
	}

	return std::unique_ptr<ActionT>(Actions);
}

bool ProtoatomPartT::IsEmpty(void) const
{
	auto &Lifted = Parent->As<CompositeT>()->Parts[0]->As<HoldPartT>()->Data;
	return Data.empty() && !Lifted && !Focused;
}

}
