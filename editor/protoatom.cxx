#include "protoatom.h"

#include "../shared/regex.h"

#include "composite.h"

namespace Core
{

ProtoatomT::ProtoatomT(CoreT &Core) : NucleusT(Core), Lifted(Core) 
{
	Visual.Tag().Add(GetTypeInfo().Tag);
}

void ProtoatomT::Serialize(Serial::WritePolymorphT &WritePolymorph) const
{
	if (IsIdentifier) WritePolymorph.Bool("IsIdentifier", *IsIdentifier);
	WritePolymorph.String("Data", Data);
	WritePolymorph.UInt("Position", Position);
	if (Lifted) Lifted->Serialize(WritePolymorph.Polymorph("Lifted"));
}
	
AtomTypeT &ProtoatomT::StaticGetTypeInfo(void)
{
	struct TypeT : AtomTypeT
	{
		TypeT(void)
		{
			Tag = "Protoatom";
		}
	} static Type;
	return Type;
}

AtomTypeT const &ProtoatomT::GetTypeInfo(void) const { return StaticGetTypeInfo(); }
	
void ProtoatomT::Focus(FocusDirectionT Direction)
{
	if (Direction == FocusDirectionT::FromBehind)
	{
		Position = 0;
		if (Lifted)
		{
			Lifted->Focus(Direction);
			return;
		}
	}
	else if (Direction == FocusDirectionT::FromAhead)
	{
		Position = Data.size();
	}
	Focused = true;
	Visual.SetClass("flag-focused");
	FlagRefresh();
	NucleusT::Focus(Direction);
}

void ProtoatomT::Defocus(void) 
{
	Focused = false;
	Visual.UnsetClass("flag-focused");
	FlagRefresh();
}

void ProtoatomT::AssumeFocus(void)
{
	Focus(FocusDirectionT::Direct);
}

void ProtoatomT::Refresh(void)
{
	Visual.Start();
	if (Lifted) Visual.Add(Lifted->Visual);
	if (Focused)
	{
		Visual.Add(Data.substr(0, Position));
		Visual.Add(Core.CursorVisual);
		Visual.Add(Data.substr(Position));
	}
	else Visual.Add(Data);
}
	
void ProtoatomT::Lift(NucleusT *Nucleus)
{
	Assert(!Lifted);
	Lifted = Nucleus;
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomT::HandleInput(InputT const &Input)
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
				ProtoatomT &Base;
				unsigned int Position;
				std::string Data;
				
				SetT(ProtoatomT &Base, unsigned int Position, std::string const &Data) : Base(Base), Position(Position), Data(Data) {}
				
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
	
	auto Result = NucleusT::HandleInput(Input);
	if (Result)
	{
		auto ActionGroup = new ActionGroupT;
		auto FinishResult = Finish({}, {}, {});
		if (FinishResult) ActionGroup->Add(std::move(*FinishResult));
		ActionGroup->Add(std::move(*Result));
		return std::unique_ptr<ActionT>(ActionGroup);
	}

	return {};
}

OptionalT<std::unique_ptr<ActionT>> ProtoatomT::Finish(
	OptionalT<AtomTypeT *> Type, 
	OptionalT<std::string> NewData, 
	OptionalT<InputT> SeedData)
{
	Assert(Atom);
	std::string Text = NewData ? *NewData : Data;
	Type = Type ? Type : Core.LookUpAtom(Data);
	auto Actions = new ActionGroupT;
	if (Type)
	{
		Assert(!SeedData);
		if (((*Type)->Arity == ArityT::Nullary) || (*Type)->Prefix)
		{
			if (Lifted) 
			{
				std::cout << "NOTE: Can't finish to nullary or prefixed op if protoatom has lifed." << std::endl; 
				return {}; 
			}
			auto Replacement = new ProtoatomT(Core);
			auto Finished = Type->Generate(Core);
			Actions->Add(Replacement->Set(Finished));
			if ((*Type)->Arity != ArityT::Nullary)
			{
				auto Proto = new ProtoatomT(Core);
				Actions->Add(Finished->Set(Proto));
			}
			Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*Atom, Replacement)));
		}
		else
		{
			auto Child = Lifted ? Lifted.Nucleus : new ProtoatomT(Core);
			Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*Atom, Child)));
			AtomT *WedgeAtom = Atom;
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
				return std::unique_ptr<ActionT>(new AtomT::SetT(*Atom, Lifted.Nucleus));
			}

			std::cout << "NOTE: Can't finish as new element if protoatom has lifted." << std::endl; 
			return {};
		}
			
		auto String = Core.StringType->Generate(Core);
		Actions->Add(String->Set(Text));

		auto Protoatom = new ProtoatomT(Core);

		auto ParentAsComposite = Parent->As<CompositeT>();
		if (ParentAsComposite && (&ParentAsComposite->TypeInfo == Core.ElementType.get()))
		{
			Protoatom->Lift(String);
		}
		else
		{
			auto Nucleus = Core.ElementType->Generate(Core);
			auto Element = Nucleus->As<CompositeT>();
			(*Element)->Parts[1]->Set(String);
			Protoatom->Lifted.Set(Nucleus);
		}

		std::cout << Core.Dump() << std::endl; // DEBUG
		Actions->Add(std::unique_ptr<ActionT>(new AtomT::SetT(*Atom, Protoatom)));

		if (SeedData) Actions->Add(Core.ActionHandleInput(*SeedData));
		std::cout << "Standard no-type finish." << std::endl;
	}

	return std::unique_ptr<ActionT>(Actions);
}

}
