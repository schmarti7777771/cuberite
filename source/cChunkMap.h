
// cChunkMap.h

// Interfaces to the cChunkMap class representing the chunk storage for a single world

#pragma once

#include "cChunk.h"





class cWorld;
class cEntity;
class MTRand;




class cChunkMap
{
public:

	static const int LAYER_SIZE = 32;

	cChunkMap(cWorld* a_World );
	~cChunkMap();

	// TODO: Get rid of these in favor of the direct action methods:
	cChunkPtr GetChunk     ( int a_ChunkX, int a_ChunkY, int a_ChunkZ );  // Also queues the chunk for loading / generating if not valid
	cChunkPtr GetChunkNoGen( int a_ChunkX, int a_ChunkY, int a_ChunkZ );  // Also queues the chunk for loading if not valid; doesn't generate
	
	// Direct action methods:
	/// Broadcasts a_Packet to all clients in the chunk where block [x, y, z] is, except to client a_Exclude
	void BroadcastToChunkOfBlock(int a_X, int a_Y, int a_Z, cPacket * a_Packet, cClientHandle * a_Exclude = NULL);
	void UseBlockEntity(cPlayer * a_Player, int a_X, int a_Y, int a_Z);  // a_Player rclked block entity at the coords specified, handle it

	void Tick( float a_Dt, MTRand & a_TickRand );

	void UnloadUnusedChunks();
	void SaveAllChunks();

	cWorld * GetWorld() { return m_World; }

	int GetNumChunks(void);
	
	/// Converts absolute block coords into relative (chunk + block) coords:
	inline static void AbsoluteToRelative(/* in-out */ int & a_X, int & a_Y, int & a_Z, /* out */ int & a_ChunkX, int & a_ChunkZ )
	{
		BlockToChunk(a_X, a_Y, a_Z, a_ChunkX, a_ChunkZ);

		a_X = a_X - a_ChunkX * 16;
		a_Z = a_Z - a_ChunkZ*16;
	}
	
	/// Converts absolute block coords to chunk coords:
	inline static void BlockToChunk( int a_X, int a_Y, int a_Z, int & a_ChunkX, int & a_ChunkZ )
	{
		(void)a_Y;
		a_ChunkX = a_X / 16;
		if ((a_X < 0) && (a_X % 16 != 0))
		{
			a_ChunkX--;
		}
		a_ChunkZ = a_Z / 16;
		if ((a_Z < 0) && (a_Z % 16 != 0))
		{
			a_ChunkZ--;
		}
	}



private:

	class cChunkLayer
	{
	public:
		cChunkLayer(int a_LayerX, int a_LayerZ, cChunkMap * a_Parent);

		/// Always returns an assigned chunkptr, but the chunk needn't be valid (loaded / generated) - callers must check
		cChunkPtr GetChunk( int a_ChunkX, int a_ChunkZ );
		
		int GetX(void) const {return m_LayerX; }
		int GetZ(void) const {return m_LayerZ; }
		int GetNumChunksLoaded(void) const {return m_NumChunksLoaded; }
		
		void Save(void);
		void UnloadUnusedChunks(void);
		
		void Tick( float a_Dt, MTRand & a_TickRand );
		
	protected:
	
		cChunkPtr m_Chunks[LAYER_SIZE * LAYER_SIZE];
		int m_LayerX;
		int m_LayerZ;
		cChunkMap * m_Parent;
		int m_NumChunksLoaded;
	};
	
	typedef std::list<cChunkLayer *> cChunkLayerList;
	// TODO: Use smart pointers for cChunkLayerList as well, so that ticking and saving needn't lock the entire layerlist
	// This however means that cChunkLayer needs to interlock its m_Chunks[]

	cChunkLayer * GetLayerForChunk( int a_ChunkX, int a_ChunkZ );  // Creates the layer if it doesn't already exist
	cChunkLayer * GetLayer( int a_LayerX, int a_LayerZ );  // Creates the layer if it doesn't already exist
	void RemoveLayer( cChunkLayer* a_Layer );

	cCriticalSection m_CSLayers;
	cChunkLayerList  m_Layers;

	cWorld * m_World;
};




