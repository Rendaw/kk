#include "protoatom.h"

#include "../shared/regex.h"

#include "logging.h"
#include "composite.h"

namespace Core
{

ProtoatomTypeT::ProtoatomTypeT(void)
{
	TRACE;
	Tag = "Protoatom";
	{
		auto Part = new AtomPartTypeT(*this);
		Part->Tag = "Lifted";
		//Part->SetDefault = true; // Unnecessary, since protoatom is only added explictly (no generic bubling sets or whatnot)
		Part->StartEmpty = true;
		Parts.emplace_back(Part);
	}
	{
		auto Part = new ProtoatomPartTypeT(*this);
		Part->Tag = "Data";
		Part->FocusDefault = true;
		Parts.emplace_back(Part);
	}
}
	
void ProtoatomPartTypeT::Serialize(Serial::WritePrepolymorphT &&Prepolymorph) const
{
	TRACE;
	Serial::WritePolymorphT Polymorph("ProtoatomPart", std::move(Prepolymorph));
	Serialize(Polymorph);
}

NucleusT *ProtoatomPartTypeT::Generate(CoreT &Core)
{
	TRACE;
	return new ProtoatomPartT(Core, *this);
}

ProtoatomPartT::ProtoatomPartT(CoreT &Core, ProtoatomPartTypeT &TypeInfo) : NucleusT(Core), TypeInfo(TypeInfo)
{
	TRACE;
	Visual.SetClass("part");
	Visual.SetClass(StringT() << "part-" << TypeInfo.Tag);
}

Serial::ReadErrorT ProtoatomPartT::Deserialize(Serial::ReadObjectT &Object)
{
	TRACE;
	Object.Bool("IsIdentifier", [this](bool Value) -> Serial::ReadErrorT { IsIdentifier = Value; return {}; });
	Object.String("Data", [this](std::string &&Value) -> Serial::ReadErrorT { Data = std::move(Value); return {}; });
	Object.UInt("Position", [this](uint64_t Value) -> Serial::ReadErrorT { Position = Value; return {}; }); // Apparently you can assign a uint64_t to a string, with no errors or warnings
	return {};
}

void ProtoatomPartT::Serialize(Serial::WritePolymorphT &Polymorph) const
{
	TRACE;
	/*Polymorph.String(::StringT() << TypeInfo.Tag << "-this", ::StringT() << this);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-parent", ::StringT() << Parent.Nucleus);
	Polymorph.String(::StringT() << TypeInfo.Tag << "-atom", ::StringT() << Atom);*/
	if (IsIdentifier) Polymorph.Bool("IsIdentifier", *IsIdentifier);
	Polymorph.String("Data", Data);
	Polymorph.UInt("Position", Position);
}

AtomTypeT const &ProtoatomPartT::GetTypeInfo(void) const { return TypeInfo; }
	
void ProtoatomPartT::Focus(FocusDirectionT Direction)
{
	TRACE;
	if (Parent->As<CompositeT>()->HasOnePart()) Core.TextMode = true;
	if (Core.TextMode)
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
		Focused = FocusedT::Text;
	}
	else
	{
		Visual.SetClass("flag-focused");
		Focused = FocusedT::On;
	}
	FlagRefresh();
	FlagStatusChange();
	NucleusT::Focus(Direction);
	if (Parent->Parent && Parent->Parent->As<AtomPartT>())
		Parent->Parent->FlagRefresh(); // Hack for hiding empty protoatoms
}

void ProtoatomPartT::Defocus(void) 
{
	TRACE;
	Focused = FocusedT::Off;
	Visual.UnsetClass("flag-focused");
	FlagRefresh();
	FlagStatusChange();
}

void ProtoatomPartT::AssumeFocus(void)
{
	TRACE;
	Focus(FocusDirectionT::Direct);
}

void ProtoatomPartT::Refresh(void)
{
	TRACE;
	Visual.Start();
	if (Focused == FocusedT::Text)
	{
		Visual.Add(Data.substr(0, Position));
		Visual.Add(Core.CursorVisual);
		Visual.Add(Data.substr(Position));
	}
	else Visual.Add(Data);
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomPartT::HandleInput(InputT const &Input)
{
	TRACE;
	AssertNE(Focused, FocusedT::Off);
	
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
			if (Data.empty()) Base.IsIdentifier.Unset();
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
		
	if (Focused == FocusedT::Text)
	{
		if (Input.Text)
		{
			auto Text = *Input.Text;

			static auto IdentifierClass = Regex::ParserT<>("[a-zA-Z0-9_]");
			auto NewIsIdentifier = IdentifierClass(Text);
			
			if (Text == " ") return Finish({}, Data); 
			else if ((Text == "\n") || (Text == ";")) 
			{
				auto Actions = new ActionGroupT;
				auto Finished = Finish({}, Data);
				if (Finished) Actions->Add(std::move(*Finished));
				auto InputResult = Parent->HandleInput(InputT{InputT::MainT::NewStatement, {}});
				if (InputResult) Actions->Add(std::move(*InputResult));
				return std::unique_ptr<ActionT>(Actions);
			}
			else if (!IsIdentifier || (*IsIdentifier == NewIsIdentifier))
			{
				if (!IsIdentifier) IsIdentifier = NewIsIdentifier;
				Assert(Text.length() == 1);
				auto NewData = Data;
				NewData.insert(Position, Text);
				auto NewPosition = Position + 1;
				if (NewPosition == NewData.size()) // Only do auto conversion if appending text
				{
					auto Found = Core.LookUpAtom(NewData);
					if (Found && Found->ReplaceImmediately) 
						return Finish(Found, NewData);
				}
				return std::unique_ptr<ActionT>(new SetT(*this, NewPosition, NewData));
			}
			else if (IsIdentifier && (*IsIdentifier != NewIsIdentifier))
			{
				Assert(!Data.empty());
				auto Finished = Finish({}, Data);
				Assert(Finished);
				auto ActionGroup = new ActionGroupT();
				ActionGroup->Add(std::move(*Finished));
				ActionGroup->Add(Core.ActionHandleInput(Input));
				return std::unique_ptr<ActionT>(ActionGroup);
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
					auto NewData = Data;
					NewData.erase(Position - 1, 1);
					auto NewPosition = Position - 1;
					return std::unique_ptr<ActionT>(new SetT(*this, NewPosition, NewData));
				}
				case InputT::MainT::Delete:
				{
					if (Position == Data.size()) return {};
					auto NewData = Data;
					NewData.erase(Position, 1);
					return std::unique_ptr<ActionT>(new SetT(*this, Position, NewData));
				}
				case InputT::MainT::Exit:
				{
					Core.TextMode = false;
					if (!Parent->As<CompositeT>()->HasOnePart())
					{
						Focus(FocusDirectionT::Direct);
						return {};
					}
					break;
				}
				default: break;
			}
		}
		
		auto Result = Parent->HandleInput(Input);
		if (Result)
		{
			auto FinishResult = Finish({}, Data);
			if (FinishResult) 
			{
				auto ActionGroup = new ActionGroupT;
				ActionGroup->Add(std::move(*FinishResult));
				ActionGroup->Add(std::move(*Result));
				return std::unique_ptr<ActionT>(ActionGroup);
			}
			else return std::move(Result);
		}

		return {};
	}
	else
	{
		if (Input.Main)
		{
			switch (*Input.Main)
			{
				case InputT::MainT::Enter:
					Core.TextMode = true;
					Focus(FocusDirectionT::Direct);
					return {};
				case InputT::MainT::Delete:
					return std::unique_ptr<ActionT>(new SetT(*this, 0, ""));
				default: break;
			}
		}
		return Parent->HandleInput(Input);
	}
}
	
bool ProtoatomPartT::IsEmpty(void) const { TRACE; return Data.empty(); }

OptionalT<std::unique_ptr<ActionT>> ProtoatomPartT::Finish(OptionalT<AtomTypeT *> Type, std::string Text)
{
	TRACE;
	Assert(Parent->Atom);
	auto &Lifted = Parent->As<CompositeT>()->Parts[0]->As<AtomPartT>()->Data;
	Type = Type ? Type : Core.LookUpAtom(Data);
	auto Actions = new ActionGroupT;
	if (Type)
	{
		if (((*Type)->Arity == ArityT::Nullary) || (*Type)->Prefix)
		{
			if (Lifted)
			{
				std::cout << "NOTE: Can't finish to nullary or prefixed op if protoatom has lifed." << std::endl; 
				return {}; 
			}
			auto Finished = Type->Generate(Core);
			Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*PartParent()->Atom, Finished)));
		}
		else
		{
			auto Child = Lifted ? Lifted.Nucleus : Core.ProtoatomType->Generate(Core);
			Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*PartParent()->Atom, Child)));
			AtomT *WedgeAtom = PartParent()->Atom;
			while ((*WedgeAtom)->PartParent() && 
				(
					((*WedgeAtom)->PartParent()->GetTypeInfo().Precedence > (*Type)->Precedence) || 
					(
						((*WedgeAtom)->PartParent()->GetTypeInfo().Precedence == (*Type)->Precedence) &&
						((*WedgeAtom)->PartParent()->GetTypeInfo().LeftAssociative)
					)
				))
			{
				std::cout << "Parent precedence " << (*WedgeAtom)->PartParent()->GetTypeInfo().Precedence << std::endl;
				WedgeAtom = (*WedgeAtom)->PartParent()->Atom;
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
		if (Data.empty())
		{
			std::cout << "NOTE: Can't finish as new element if empty." << std::endl; 
			return {};
		}

		if (Lifted) 
		{
			if (Data.empty())
			{
				Assert(Atom);
				std::cout << "Replacing " << this << " with " << Lifted.Nucleus << ", atom " << Atom << std::endl;
				return std::unique_ptr<ActionT>(new AtomT::SetT(*PartParent()->Atom, Lifted.Nucleus));
			}

			std::cout << "NOTE: Can't finish as new element if protoatom has lifted." << std::endl; 
			return {};
		}
		
		auto String = Core.StringType->Generate(Core);
		Actions->Add(String->Set(Text));

		auto Protoatom = Core.ProtoatomType->Generate(Core);

		auto ParentAsComposite = PartParent()->As<CompositeT>();
		if (ParentAsComposite && (&ParentAsComposite->TypeInfo == Core.ElementType))
		{
			Actions->Add(Protoatom->As<CompositeT>()->Parts[0]->As<AtomPartT>()->Set(String));
		}
		else
		{
			auto Element = Core.ElementType->Generate(Core);
			Actions->Add(Element->As<CompositeT>()->Parts[1]->Set(String));
			Actions->Add(Protoatom->As<CompositeT>()->Parts[0]->As<AtomPartT>()->Set(Element));
		}

		std::cout << Core.Dump() << std::endl; // DEBUG
		Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*PartParent()->Atom, Protoatom)));

		std::cout << "Standard no-type finish." << std::endl;
	}

	return std::unique_ptr<ActionT>(Actions);
}

}
