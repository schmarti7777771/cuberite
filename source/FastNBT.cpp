
// FastNBT.cpp

// Implements the fast NBT parser and writer

#include "Globals.h"
#include "FastNBT.h"





#define RETURN_FALSE_IF_FALSE(X) do { if (!X) return false; } while (0)





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// cFastNBTParser:

#define NEEDBYTES(N) \
	if (m_Length - m_Pos < N) \
	{ \
		return false; \
	}





cParsedNBT::cParsedNBT(const char * a_Data, int a_Length) :
	m_Data(a_Data),
	m_Length(a_Length),
	m_Pos(0)
{
	m_IsValid = Parse();
}





bool cParsedNBT::Parse(void)
{
	if (m_Length < 3)
	{
		// Data too short
		return false;
	}
	if (m_Data[0] != TAG_Compound)
	{
		// The top-level tag must be a Compound
		return false;
	}
	
	m_Tags.reserve(200);
	
	m_Tags.push_back(cFastNBTTag(TAG_Compound, -1));
	
	m_Pos = 1;
	
	RETURN_FALSE_IF_FALSE(ReadString(m_Tags.back().m_NameStart, m_Tags.back().m_NameLength)); 
	RETURN_FALSE_IF_FALSE(ReadCompound());
	
	return true;
}





bool cParsedNBT::ReadString(int & a_StringStart, int & a_StringLen)
{
	NEEDBYTES(2);
	a_StringStart = m_Pos + 2;
	a_StringLen = ntohs(*((short *)(m_Data + m_Pos)));
	if (a_StringLen < 0)
	{
		// Invalid string length
		return false;
	}
	m_Pos += 2 + a_StringLen;
	return true;
}





bool cParsedNBT::ReadCompound(void)
{
	// Reads the latest tag as a compound
	int ParentIdx = m_Tags.size() - 1;
	int PrevSibling = -1;
	while (true)
	{
		NEEDBYTES(1);
		eTagType TagType = (eTagType)(m_Data[m_Pos]);
		m_Pos++;
		if (TagType == TAG_End)
		{
			break;
		}
		m_Tags.push_back(cFastNBTTag(TagType, ParentIdx, PrevSibling));
		if (PrevSibling >= 0)
		{
			m_Tags[PrevSibling].m_NextSibling = m_Tags.size() - 1;
		}
		else
		{
			m_Tags[ParentIdx].m_FirstChild = m_Tags.size() - 1;
		}
		PrevSibling = m_Tags.size() - 1;
		RETURN_FALSE_IF_FALSE(ReadString(m_Tags.back().m_NameStart, m_Tags.back().m_NameLength));
		RETURN_FALSE_IF_FALSE(ReadTag());
	}  // while (true)
	m_Tags[ParentIdx].m_LastChild = PrevSibling;
	return true;
}





bool cParsedNBT::ReadList(eTagType a_ChildrenType)
{
	// Reads the latest tag as a list of items of type a_ChildrenType
	
	// Read the count:
	NEEDBYTES(4);
	int Count = ntohl(*((int *)(m_Data + m_Pos)));
	m_Pos += 4;
	if (Count < 0)
	{
		return false;
	}

	// Read items:
	int ParentIdx = m_Tags.size() - 1;
	int PrevSibling = -1;
	for (int i = 0; i < Count; i++)
	{
		m_Tags.push_back(cFastNBTTag(a_ChildrenType, ParentIdx, PrevSibling));
		if (PrevSibling >= 0)
		{
			m_Tags[PrevSibling].m_NextSibling = m_Tags.size() - 1;
		}
		else
		{
			m_Tags[ParentIdx].m_FirstChild = m_Tags.size() - 1;
		}
		PrevSibling = m_Tags.size() - 1;
		RETURN_FALSE_IF_FALSE(ReadTag());
	}  // for (i)
	m_Tags[ParentIdx].m_LastChild = PrevSibling;
	return true;
}





#define CASE_SIMPLE_TAG(TAGTYPE, LEN) \
	case TAG_##TAGTYPE: \
	{ \
		NEEDBYTES(LEN); \
		Tag.m_DataStart = m_Pos; \
		Tag.m_DataLength = LEN; \
		m_Pos += LEN; \
		return true; \
	}
	
bool cParsedNBT::ReadTag(void)
{
	cFastNBTTag & Tag = m_Tags.back();
	switch (Tag.m_Type)
	{
		CASE_SIMPLE_TAG(Byte,   1)
		CASE_SIMPLE_TAG(Short,  2)
		CASE_SIMPLE_TAG(Int,    4)
		CASE_SIMPLE_TAG(Long,   8)
		CASE_SIMPLE_TAG(Float,  4)
		CASE_SIMPLE_TAG(Double, 8)
		
		case TAG_String:
		{
			return ReadString(Tag.m_DataStart, Tag.m_DataLength);
		}
		
		case TAG_ByteArray:
		{
			NEEDBYTES(4);
			int len = ntohl(*((int *)(m_Data + m_Pos)));
			m_Pos += 4;
			if (len < 0)
			{
				// Invalid length
				return false;
			}
			NEEDBYTES(len);
			Tag.m_DataLength = len;
			Tag.m_DataStart = m_Pos;
			m_Pos += len;
			return true;
		}
		
		case TAG_List:
		{
			NEEDBYTES(1);
			eTagType ItemType = (eTagType)m_Data[m_Pos];
			m_Pos++;
			RETURN_FALSE_IF_FALSE(ReadList(ItemType));
			return true;
		}
		
		case TAG_Compound:
		{
			RETURN_FALSE_IF_FALSE(ReadCompound());
			return true;
		}
		
		case TAG_IntArray:
		{
			NEEDBYTES(4);
			int len = ntohl(*((int *)(m_Data + m_Pos)));
			m_Pos += 4;
			if (len < 0)
			{
				// Invalid length
				return false;
			}
			len *= 4;
			NEEDBYTES(len);
			Tag.m_DataLength = len;
			Tag.m_DataStart = m_Pos;
			m_Pos += len;
			return true;
		}
		
		default:
		{
			ASSERT(!"Unhandled NBT tag type");
			return false;
		}
	}  // switch (iType)
}

#undef CASE_SIMPLE_TAG





int cParsedNBT::FindChildByName(int a_Tag, const char * a_Name, size_t a_NameLength) const
{
	if (a_Tag < 0)
	{
		return -1;
	}
	if (m_Tags[a_Tag].m_Type != TAG_Compound)
	{
		return -1;
	}
	
	if (a_NameLength == 0)
	{
		a_NameLength = strlen(a_Name);
	}
	for (int Child = m_Tags[a_Tag].m_FirstChild; Child != -1; Child = m_Tags[Child].m_NextSibling)
	{
		if (
			(m_Tags[Child].m_NameLength == a_NameLength) &&
			(memcmp(m_Data + m_Tags[Child].m_NameStart, a_Name, a_NameLength) == 0)
		)
		{
			return Child;
		}
	}  // for Child - children of a_Tag
	return -1;
}





int cParsedNBT::FindTagByPath(int a_Tag, const AString & a_Path) const
{
	if (a_Tag < 0)
	{
		return -1;
	}
	size_t Begin = 0;
	size_t Length = a_Path.length();
	int Tag = a_Tag;
	for (size_t i = 0; i < Length; i++)
	{
		if (a_Path[i] != '\\')
		{
			continue;
		}
		Tag = FindChildByName(Tag, a_Path.c_str() + Begin, i - Begin - 1);
		if (Tag < 0)
		{
			return -1;
		}
		Begin = i + 1;
	}  // for i - a_Path[]
	
	if (Begin < Length)
	{
		Tag = FindChildByName(Tag, a_Path.c_str() + Begin, Length - Begin);
	}
	return Tag;
}





