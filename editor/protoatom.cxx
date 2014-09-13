#include "protoatom.h"

#include "../shared/regex.h"

#include "logging.h"
#include "composite.h"
	
static auto AnyClass = std::regex("[a-zA-Z0-9:!@#$%^&*()<>,./\\[\\]{}|\\`~-_'\"=+]");
static auto IdentifierClass = Regex::ParserT<>("^[a-zA-Z0-9_]+$");

namespace Core
{

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

	if ((Focused == FocusedT::Off) && (Parent->As<CompositeT>()->HasOnePart())) 
		Core.TextMode = true;

	if (Core.TextMode)
	{
		if (Focused != FocusedT::Text)
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
	}
	else 
	{
		if (Focused != FocusedT::On)
		{
			Visual.SetClass("flag-focused");
			Focused = FocusedT::On;
		}
	}
	FlagRefresh();
	FlagStatusChange();
	NucleusT::Focus(Direction);
	/*if (Parent->Parent && Parent->Parent->As<AtomPartT>())
		Parent->Parent->FlagRefresh(); // Hack for hiding empty protoatoms*/
}
	
void ProtoatomPartT::RegisterActions(void)
{
	TRACE;
	
	if (Focused == FocusedT::Text)
	{
		struct FinishT : ActionT
		{
			ProtoatomPartT &Base;
			FinishT(ProtoatomPartT &Base) : ActionT("Finish"), Base(Base) { }
			void Apply(void)
			{
				TRACE;
				Base.Finish({}, Base.Data);
			}
		};
		Core.RegisterAction(make_unique<FinishT>(*this));

		struct TextT : ActionT
		{
			ProtoatomPartT &Base;
			ActionT::TextArgumentT Argument;
			TextT(ProtoatomPartT &Base) : ActionT("Enter text"), Base(Base)
			{ 
				Argument.Regex = AnyClass;
				Arguments.push_back(&Argument); 
			}

			void Apply(void)
			{
				TRACE;
				Base.HandleText(Argument.Data);
			}
		};
		Core.RegisterAction(make_unique<TextT>(*this));

		struct FocusPreviousT : ActionT
		{
			ProtoatomPartT &Base;
			FocusPreviousT(ProtoatomPartT &Base) : ActionT("Left"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				if (Base.Position == 0) 
				{
					Base.Parent->FocusPrevious();
				}
				else
				{
					Base.Position -= 1;
					Base.FlagRefresh();
				}
			}
		};
		Core.RegisterAction(make_unique<FocusPreviousT>(*this));

		struct FocusNextT : ActionT
		{
			ProtoatomPartT &Base;
			FocusNextT(ProtoatomPartT &Base) : ActionT("Right"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				if (Base.Position == Base.Data.size()) 
				{
					Base.Parent->FocusNext();
				}
				else
				{
					Base.Position += 1;
					Base.FlagRefresh();
				}
			}
		};
		Core.RegisterAction(make_unique<FocusNextT>(*this));

		struct BackspaceT : ActionT
		{
			ProtoatomPartT &Base;
			BackspaceT(ProtoatomPartT &Base) : ActionT("Backspace"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				if (Base.Position == 0) return;
				auto NewData = Base.Data;
				NewData.erase(Base.Position - 1, 1);
				auto NewPosition = Base.Position - 1;
				Base.Set(NewPosition, NewData);
			}
		};
		Core.RegisterAction(make_unique<BackspaceT>(*this));

		struct DeleteT : ActionT
		{
			ProtoatomPartT &Base;
			DeleteT(ProtoatomPartT &Base) : ActionT("Delete"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				if (Base.Position == Base.Data.size()) return;
				auto NewData = Base.Data;
				NewData.erase(Base.Position, 1);
				Base.Set(Base.Position, NewData);
			}
		};
		Core.RegisterAction(make_unique<DeleteT>(*this));

		if (!Parent->As<CompositeT>()->HasOnePart())
		{
			struct ExitT : ActionT
			{
				ProtoatomPartT &Base;
				ExitT(ProtoatomPartT &Base) : ActionT("Exit"), Base(Base) {}
				void Apply(void)
				{
					TRACE;
					Base.Core.TextMode = false;
					Base.Focus(FocusDirectionT::Direct);
				}
			};
			Core.RegisterAction(make_unique<ExitT>(*this));
		}
	}
	else if (Focused == FocusedT::On)
	{
		struct DeleteT : ActionT
		{
			ProtoatomPartT &Base;
			DeleteT(ProtoatomPartT &Base) : ActionT("Delete"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				Base.Set(0, "");
			}
		};
		Core.RegisterAction(make_unique<DeleteT>(*this));

		struct EnterT : ActionT
		{
			ProtoatomPartT &Base;
			EnterT(ProtoatomPartT &Base) : ActionT("Enter"), Base(Base) {}
			void Apply(void)
			{
				TRACE;
				Base.Core.TextMode = true;
				Base.Focus(FocusDirectionT::Direct);
			}
		};
		Core.RegisterAction(make_unique<EnterT>(*this));
	}
	Parent->RegisterActions();
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

bool ProtoatomPartT::IsEmpty(void) const { TRACE; return Data.empty(); }
	
void ProtoatomPartT::Set(size_t NewPosition, std::string const &NewData)
{
	TRACE;
	struct SetT : ReactionT
	{
		ProtoatomPartT &Base;
		unsigned int Position;
		std::string Data;
		
		SetT(ProtoatomPartT &Base, unsigned int Position, std::string const &Data) : Base(Base), Position(Position), Data(Data) {}
		
		void Apply(void) { Base.Set(Position, Data); }
		
		bool Combine(std::unique_ptr<ReactionT> &Other) override
		{
			auto Set = dynamic_cast<SetT *>(Other.get());
			if (!Set) return false;
			if (&Set->Base != &Base) return false;
			Position = Set->Position;
			Data = Set->Data;
			return true;
		}
	};

	Core.AddUndoReaction(make_unique<SetT>(*this, Position, Data));
	Data = NewData;
	Position = NewPosition;
	if (Data.empty()) IsIdentifier.Unset();
	else if (!IsIdentifier) IsIdentifier = IdentifierClass(Data);
	FlagRefresh();
}

void ProtoatomPartT::HandleText(std::string const &Text)
{
	TRACE;
	// TODO this should handle arbitrary length text strings

	auto NewIsIdentifier = IdentifierClass(Text);
	
	if (!IsIdentifier || (*IsIdentifier == NewIsIdentifier))
	{
		Assert(Text.length() == 1);
		auto NewData = Data;
		NewData.insert(Position, Text);
		auto NewPosition = Position + 1;
		if (NewPosition == NewData.size()) // Only do auto conversion if appending text
		{
			auto Found = Core.LookUpAtomType(NewData);
			if (Found && Found->ReplaceImmediately) 
			{
				Finish(Found, NewData);
				return;
			}
		}
		Set(NewPosition, NewData);
	}
	else if (IsIdentifier && (*IsIdentifier != NewIsIdentifier))
	{
		Assert(!Data.empty());
		auto Finished = Finish({}, Data);
		Assert(Finished);
		auto Protoatom = AsProtoatom(Finished);
		Assert(Protoatom);
		Protoatom->HandleText(Text);
	}
}

OptionalT<NucleusT *> ProtoatomPartT::Finish(OptionalT<AtomTypeT *> Type, std::string Text)
{
	TRACE;
	Assert(Parent->Atom);
	auto &Lifted = GetProtoatomLiftedPart(Parent.Nucleus)->Data;
	Type = Type ? Type : Core.LookUpAtomType(Data);
	if (Type)
	{
		if (((*Type)->Arity == ArityT::Nullary) || (*Type)->Prefix)
		{
			if (Lifted)
			{
				std::cout << "NOTE: Can't finish to nullary or prefixed op if protoatom has lifed." << std::endl; 
				return {}; 
			}
			PartParent()->Atom->Set(Type->Generate(Core));
			return {};
		}
		else
		{
			auto Protoatom = Lifted ? nullptr : Core.ProtoatomType->Generate(Core);
			auto Child = Lifted ? Lifted.Nucleus : Protoatom;
			AtomT *WedgeAtom = PartParent()->Atom;
			PartParent()->Atom->Set(Child);
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
			WedgeAtom->Set(Finished);
			{
				auto Composite = Finished->As<CompositeT>();
				bool Placed = false;
				for (auto &Part : Composite->Parts)
				{
					auto AtomPart = (*Part->Atom)->As<AtomPartT>();
					if (AtomPart) 
					{
						AtomPart->Data.Set(Child);
						Placed = true;
						break;
					}
				}
				Assert(Placed);
			}
			return Protoatom;
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
				PartParent()->Atom->Set(Lifted.Nucleus);
				return {};
			}

			std::cout << "NOTE: Can't finish as new element if protoatom has lifted." << std::endl; 
			return {};
		}
		
		auto String = Core.StringType->Generate(Core);
		GetStringPart(String)->Set(0, Text);

		auto Protoatom = Core.ProtoatomType->Generate(Core);

		auto ParentAsComposite = PartParent()->As<CompositeT>();
		if (ParentAsComposite && (&ParentAsComposite->TypeInfo == Core.ElementType))
		{
			GetProtoatomLiftedPart(Protoatom)->Data.Set(String);
		}
		else
		{
			auto Element = Core.ElementType->Generate(Core);
			GetElementRightPart(Element)->Data.Set(String);
			GetProtoatomLiftedPart(Protoatom)->Data.Set(Element);
		}

		PartParent()->Atom->Set(Protoatom);

		std::cout << Core.Dump() << std::endl; // DEBUG
		std::cout << "Standard no-type finish." << std::endl;
		return Protoatom;
	}
}

void CheckProtoatomType(AtomTypeT *Type)
{
	auto Composite = dynamic_cast<CompositeTypeT *>(Type);
	Assert(Composite);
	if (!AssertE(Composite->Parts.size(), 2)) throw ConstructionErrorT() << "Protoatom must have 2 parts.";
	if (!Assert(dynamic_cast<AtomPartTypeT *>(Composite->Parts[0].get()))) throw ConstructionErrorT() << "Protoatom part 1 must be an atom.";
	if (!Assert(dynamic_cast<AtomPartTypeT *>(Composite->Parts[0].get())->StartEmpty)) throw ConstructionErrorT() << "The Protoatom atom part must be StartEmpty.";
	if (!(dynamic_cast<ProtoatomPartTypeT *>(Composite->Parts[1].get()))) throw ConstructionErrorT() << "Protoatom part 2 must be a \"protoatom\" part.";
}

ProtoatomPartT *GetProtoatomPart(NucleusT *Protoatom)
{
	return *Protoatom->As<CompositeT>()->Parts[1]->As<ProtoatomPartT>();
}

AtomPartT *GetProtoatomLiftedPart(NucleusT *Protoatom)
{
	return *Protoatom->As<CompositeT>()->Parts[0]->As<AtomPartT>();
}

}
