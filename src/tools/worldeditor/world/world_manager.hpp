/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WORLD_MANGER_HPP
#define WORLD_MANGER_HPP


#include "worldeditor/config.hpp"
#include "worldeditor/forward.hpp"
#include "worldeditor/world/editor_chunk_item_linker_manager.hpp"
#include "worldeditor/terrain/editor_chunk_terrain.hpp"
#include "worldeditor/project/world_editord_connection.hpp"
#include "worldeditor/import/terrain_utils.hpp"
#include "worldeditor/editor/item_view.hpp"
#include "worldeditor/misc/editor_tickable.hpp"
#include "worldeditor/misc/editor_renderable.hpp"
#include "worldeditor/misc/world_editor_progress_bar.hpp"
#include "guimanager/gui_action_maker.hpp"
#include "guimanager/gui_updater_maker.hpp"
#include "guimanager/gui_functor_option.hpp"
#include "common/romp_harness.hpp"
#include "common/space_mgr.hpp"
#include "input/input.hpp"
#include "gizmo/snap_provider.hpp"
#include "gizmo/coord_mode_provider.hpp"
#include "gizmo/tool.hpp"
#include "gizmo/undoredo.hpp"
#include "math/vector3.hpp"
#include "cstdmf/concurrency.hpp"
#include "cstdmf/slow_task.hpp"
#include <string>
#include <set>
#include <stack>



struct WorldEditorDebugMessageCallback : public DebugMessageCallback
{
	virtual bool handleMessage( int componentPriority,
		int messagePriority, const char * format, va_list argPtr );
};

/**
 *	This class is the highest authority in the WorldEditor
 */
class WorldManager : public SnapProvider, public CoordModeProvider, public ReferenceCount,
	SlowTaskHandler,
	GUI::ActionMaker<WorldManager>, GUI::ActionMaker<WorldManager, 1>, 
	GUI::ActionMaker<WorldManager, 2>, GUI::ActionMaker<WorldManager, 3>, 
	GUI::ActionMaker<WorldManager, 4>, GUI::ActionMaker<WorldManager, 5>,
	GUI::ActionMaker<WorldManager, 6>, GUI::ActionMaker<WorldManager, 7>, 
	GUI::ActionMaker<WorldManager, 8>, GUI::ActionMaker<WorldManager, 9>,
	GUI::OptionMap,
	GUI::UpdaterMaker<WorldManager>, GUI::UpdaterMaker<WorldManager, 1>, 
	GUI::UpdaterMaker<WorldManager, 2>,	GUI::UpdaterMaker<WorldManager, 3>
{
public:
	typedef WorldManager This;

	static WorldManager& instance();
    ~WorldManager();

	bool	init( HINSTANCE hInst, HWND hwndInput, HWND hwndGraphics );
	bool	postLoadThreadInit();
	void	fini();
	void	forceClean();
	bool	isDirty() const;
	bool	canClose( const std::string& action );

	// tool mode updater called by the tool pages
	void updateUIToolMode( const std::string& pyID );

    //commands and properties.  In fini, all of these will
	//have a gui item associated with them.  All gui items
	//will call through to these properties and functions.
    void	timeOfDay( float t );
    void	rainAmount( float a );
	void	farPlane( float f );
	float	farPlane() const;
	float	dTime() const	{ return dTime_; }
    void	focus( bool state );
    void	globalWeather( bool state )	{ globalWeather_ = state; }
    void	propensity( const std::string& weatherSystemName, float amount );

	void	addCommentaryMsg( const std::string& msg, int id = 0 );
	// notify user of an error (gui errors pane & commentary)
	void	addError(Chunk* chunk, ChunkItem* item, const char * format, ...);

	void	reloadAllChunks( bool askBeforeProceed );
	void	changedChunk( Chunk * pChunk, bool rebuildNavmesh = true );
	void	dirtyThumbnail( Chunk * pChunk, bool justLoaded = false );
	void	changedTerrainBlock( Chunk * pChunk, bool rebuildNavmesh = true );
	void				recordLoadedChunksStart();
	std::set<Chunk*>	recordLoadedChunksStop();
	void	chunkShadowUpdated( Chunk * pChunk );
	void	resaveAllTerrainBlocks();
	void	restitchAllTerrainBlocks();
	void	regenerateThumbnailsOffline();
	void	regenerateLODsOffline();
	void	convertSpaceToZip();
	void	optimiseTerrainTextures();
	void	chunkThumbnailUpdated( Chunk * pChunk );
	void	changedTerrainBlockOffline( const std::string& chunkName );
	void	resetChangedLists();
	bool	checkForReadOnly() const;
	bool	EnsureNeighbourChunkLoaded( Chunk* chunk, int level );
	bool	EnsureNeighbourChunkLoadedForShadow( Chunk* chunk );

	void	loadNeighbourChunk( Chunk* chunk, int level );
	void	loadChunkForShadow( Chunk* chunk );
	bool	saveChunk( const std::string& chunkName, ProgressTask& task );
	bool	saveChunk( Chunk* chunk, ProgressTask& task );
	
	void	save( const std::set<std::string>* chunkToSave = NULL, bool recalcOnly = false );
	void	quickSave();

    void    environmentChanged() { changedEnvironment_ = true; }

	bool	snapsEnabled() const;
	bool	freeSnapsEnabled() const;
	bool	terrainSnapsEnabled() const;
	bool	obstacleSnapsEnabled() const;
	Vector3	movementSnaps() const;
	float	angleSnaps() const;

	/** 0 = no, 1 = some, 2 = all */
	int	drawBSP() const;

	virtual SnapMode snapMode( ) const;
	virtual bool snapPosition( Vector3& v );
	virtual Vector3 snapNormal( const Vector3& v );
	virtual void snapPositionDelta( Vector3& v );
	virtual void snapAngles( Matrix& v );
	virtual float angleSnapAmount();

	virtual void startSlowTask();
	virtual void stopSlowTask();

	virtual CoordMode getCoordMode() const;

	const ChunkWatcher &chunkWatcher() const { return *chunkWatcher_; }

	//utility methods
	void	update( float dTime );
	void	checkMemoryLoad();
	void	writeStatus();
	
	//app modules can call any of these methods in whatever order they desire.
	//must call beginRender() and endRender() though ( for timing info )
	void	beginRender();
    void	renderRompPreScene();
	void	renderChunks();
	void	renderTerrain( float dTime );
	void	renderEditorGizmos();
	void	tickEditorTickables();
	void	renderEditorRenderables();
	void	renderDebugGizmos();	
	void	renderRompDelayedScene();
	void	renderRompPostScene();
	void	endRender();
	//or app modules can call this on its own
	void	render( float dTime );

	// these methods add/remove custom tickable objects
	void addTickable( EditorTickablePtr tickable );
	void removeTickable( EditorTickablePtr tickable );

	// these methods add/remove custom renderable objects
	void addRenderable( EditorRenderablePtr renderable );
	void removeRenderable( EditorRenderablePtr renderable );

	/** The lights influencing the chunk have changed, flag it for recalculation */
	void	dirtyLighting( Chunk * pChunk );

	/**
	 * If the given chunk is scheduled to have it's lighting recalculated
	 */
	bool	isDirtyLightChunk( Chunk * pChunk );

	/** Mark the given chunk, and everything 500m either side along the x axis dirty */
	void	markTerrainShadowsDirty( Chunk * pChunk );
	/** Mark the chunks in the given area, and everything 500m either side along the x axis dirty */
	void	markTerrainShadowsDirty( const BoundingBox& bb );

	/** Mark the chunk as it's being edited, to avoid doing background calculations on it. */
	void	lockChunkForEditing( Chunk * pChunk, bool editing );


	/**
	 * Mark the chunk as dirty for terrain and lighting if it wasn't fully
	 * saved last session
	 */
	void	checkUpToDate( Chunk * pChunk );

	/**
	 * The chunk is about to be ejected.
	 */
	void	onEjectChunk( Chunk * pChunk );

	/**
	 * Called by sub fibers to indicate that they'll give up their time slot
	 * now if they're out of time
	 *
	 * Returns if we actually paused or not
	 */
	bool	fiberPause();

    /**
     * Temporarily stop background processing.
     */
    void stopBackgroundCalculation();

	//saving, adding, writing, erasing, removing, deleting low level fns
	struct SaveableObjectBase { virtual bool save( const std::string & ) const = 0; };

	bool	saveAndAddChunkBase( const std::string & resourceID,
		const SaveableObjectBase & saver, bool add, bool addAsBinary );

	void	eraseAndRemoveFile( const std::string & resourceID );

	//2D - 3D mouse mapping
	const Vector3& worldRay() const		{ return worldRay_; }

	HWND hwndGraphics() const			{ return hwndGraphics_; }

	// Test to see if the escape key was pressed, and update information accordingly.
	bool escapePressed();
	// Test for valid mouse position
	bool cursorOverGraphicsWnd() const;
	// Current mouse position in the window
	POINT currentCursorPosition() const;
	// Calculate a world ray from the given position
	Vector3 getWorldRay(POINT& pt) const;
	Vector3 getWorldRay(int x, int y) const;

	// The connection to bigbangd, used for locking etc
	BWLock::WorldEditordConnection& connection();

	// Add the block to the list that will be rendered read only
	void addReadOnlyBlock( const Matrix& transform, Terrain::BaseTerrainBlockPtr pBlock );
	// Setup the fog to draw stuff readonly, ie, 50% red
	void setReadOnlyFog();

	bool isPointInWriteableChunk( const Vector3& pt ) const;	
	bool isBoundingBoxInWriteableChunk( const BoundingBox& box, const Vector3& offset ) const;	

	bool warnSpaceNotLocked();

	DebugMessageCallback * getDebugMessageCallback() { return &debugMessageCallback_; }

	static bool messageHandler( int componentPriority, int messagePriority,
		const char * format, va_list argPtr );

	// check if a particular item is selected
	bool isItemSelected( ChunkItemPtr item ) const;
	// if the shell model for the given chunk is in the selection
	bool isChunkSelected( Chunk * pChunk ) const;
	// if there is a chunk in the selection
	bool isChunkSelected() const;
	// if any items in the given chunk are selected
	bool isItemInChunkSelected( Chunk * pChunk ) const;

	bool isInPlayerPreviewMode() const;
	void setPlayerPreviewMode( bool enable );

	bool touchAllChunks();

	std::string getCurrentSpace() const;

	void markChunks();
	void unloadChunks();

	void setSelection( const std::vector<ChunkItemPtr>& items, bool updateSelection = true );
	void getSelection();

	bool drawSelection() const;
	void drawSelection( bool drawingSelection );
	void registerDrawSelectionItem( EditorChunkItem* item );
	bool isDrawSelectionItemRegistered( EditorChunkItem* item ) const;

	const std::vector<ChunkItemPtr>& selectedItems() const { return selectedItems_; }

	ChunkDirMapping* chunkDirMapping();

	// notify BB that n prim groups were just drawn in the given chunk
	// used to calculate data for the status bar
	void addPrimGroupCount( Chunk* chunk, uint n );

	// get time of day and environment minder and original seconds per hour
	TimeOfDay* timeOfDay() { return romp_->timeOfDay(); }
    EnviroMinder &enviroMinder() { return romp_->enviroMinder(); }
    float secondsPerHour() const { return secsPerHour_; }
    void secondsPerHour( float value ) { secsPerHour_ = value; }
	void refreshWeather();

	void setStatusMessage( unsigned int index, const std::string& value );
	const std::string& getStatusMessage( unsigned int index ) const;

	void loadChunkForThumbnail( const std::string& chunkName );
	void discardChunkForThumbnail( Chunk * pChunk );

	// you can save anything!
	template <class C> struct SaveableObjectPtr : public SaveableObjectBase
	{
		SaveableObjectPtr( C & ob ) : ob_( ob ) { }
		bool save( const std::string & chunkID ) const { return ob_->save( chunkID ); }
		C & ob_;
	};
	template <class C> bool saveAndAddChunk( const std::string & resourceID,
		C saver, bool add, bool addAsBinary )
	{
		return this->saveAndAddChunkBase( resourceID, SaveableObjectPtr<C>(saver),
			add, addAsBinary );
	}

	Chunk* workingChunk() const
	{
		return workingChunk_;
	}
	void workingChunk( Chunk* chunk, bool canEject );
	bool isWorkingChunk( Chunk* chunk )
	{
		return chunk && workingChunk_ == chunk;
	}

	void setCursor( HCURSOR cursor );
	void resetCursor();
	HCURSOR cursor() const
	{
		return cursor_;
	}

	unsigned int dirtyChunks() const;
	unsigned int dirtyLODTextures() const;
	static unsigned int getMemoryLoad();

	// Methods related to terrain painting
	void startLODTextureRegen();
	void stopLODTextureRegen();
	void chunkTexturesPainted( Chunk* chunk, bool rebuiltLodTexture );
	void chunkTexturesContextMenu( Chunk* chunk );

	uint32								getTerrainVersion();
	Terrain::BaseTerrainBlockPtr		getTerrainBlock();
	const TerrainUtils::TerrainFormat&	getTerrainInfo();
	void								resetTerrainInfo();
	Terrain::TerrainSettingsPtr			pTerrainSettings();

	WorldEditorProgressBar*	progressBar() { return progressBar_; }

	EditorChunkItemLinkableManager& linkerManager() { return linkerManager_; }

	//-------------------------------------------------
	//Python Interface
	//-------------------------------------------------

	PY_MODULE_STATIC_METHOD_DECLARE( py_worldRay )
	PY_MODULE_STATIC_METHOD_DECLARE( py_markAllChunksClean )
	PY_MODULE_STATIC_METHOD_DECLARE( py_repairTerrain )
	PY_MODULE_STATIC_METHOD_DECLARE( py_farPlane )

	PY_MODULE_STATIC_METHOD_DECLARE( py_save )
	PY_MODULE_STATIC_METHOD_DECLARE( py_quickSave )
	PY_MODULE_STATIC_METHOD_DECLARE( py_update )
	PY_MODULE_STATIC_METHOD_DECLARE( py_render )

	PY_MODULE_STATIC_METHOD_DECLARE( py_revealSelection )
	PY_MODULE_STATIC_METHOD_DECLARE( py_isChunkSelected )
	PY_MODULE_STATIC_METHOD_DECLARE( py_selectAll )

	PY_MODULE_STATIC_METHOD_DECLARE( py_cursorOverGraphicsWnd )
    PY_MODULE_STATIC_METHOD_DECLARE( py_importDataGUI )
    PY_MODULE_STATIC_METHOD_DECLARE( py_exportDataGUI )

	PY_MODULE_STATIC_METHOD_DECLARE( py_rightClick )

	// Constants
	static const int TIME_OF_DAY_MULTIPLIER = 10;

	void	startBackgroundProcessing();
	void	endBackgroundProcessing();

	float	getMaxFarPlane() const;

	typedef std::set<std::string> ChunkSet;

	void registerDelayedChanges();

	static void processMessages();
private:
	WorldManager();

	HANDLE	spaceLock_;
	bool	inited_;
	bool	updating_;
    bool	chunkManagerInited_;

	Chunk*	workingChunk_;
	bool	canEjectChunk_;

    bool	initRomp();

    RompHarness * romp_;
    float	dTime_;
	bool	canSeeTerrain_;

    std::string	projectPath_;
	bool	isInPlayerPreviewMode_;
    bool	globalWeather_;
    double	totalTime_;
    HWND	hwndInput_;
    HWND	hwndGraphics_;

	std::set<Chunk *> changedChunks_;
    std::set<Terrain::EditorBaseTerrainBlockPtr> changedTerrainBlocks_;
	std::set<Chunk *> changedThumbnailChunks_;
	std::set<Chunk *> thumbnailChunksLoading_;
    bool changedEnvironment_;
    float secsPerHour_;

	bool saveChangedFiles( SuperModelProgressDisplay& progress );

	// This is private as nobody should call it directly, use
	// markTerrainShadowsDirty instead (which flags the affected
	// row of chunks as dirty too.)
	void dirtyTerrainShadows( Chunk * pChunk );

	/**
	 * Called once per frame to recalc static lighting, terrain shadows, etc
	 */
	void	doBackgroundUpdating();

	/** Which chunks need their lighting recalculated */
	std::vector<Chunk*> dirtyLightingChunks_;

	/**
	 * As above, but contains names of chunks that haven't yet been loaded,
	 * when they do get loaded, they'll be removed from here and added to the
	 * above list
	 */
	ChunkSet nonloadedDirtyLightingChunks_;

	/** Which outside chunks need to have their terrain shadows recalculated */
	std::vector<Chunk*> dirtyTerrainShadowChunks_;

	/** Which outside chunks need to have their terrain shadows recalculated */
	std::set<Chunk*> chunksBeingEdited_;

	/**
	 * As above, but contains names of chunks that haven't yet been loaded,
	 * when they do get loaded, they'll be removed from here and added to the
	 * above list
	 */
	ChunkSet nonloadedDirtyTerrainShadowChunks_;

	/** Which chunks need their thumbnail recalculated */
	std::vector<Chunk*> dirtyThumbnailChunks_;

	/** Which chunks need their texture lods recalculated */
	std::set<Chunk*> dirtyTextureLodChunks_;
	size_t lodRegenCount_;

	/** Names of chunks not yet loaded that have dirty terrain texture LODs. */
	ChunkSet nonloadedDirtyTextureLodChunks_;

	/**
	 * As above, but contains names of chunks that haven't yet been loaded,
	 * when they do get loaded, they'll be removed from here and added to the
	 * above list
	 */
	ChunkSet nonloadedDirtyThumbnailChunks_;

	bool recordLoadedChunks_;
	std::set<Chunk*> loadedChunks_;

	/**
	 *	Set of custom tickable objects.
	 */
	std::list<EditorTickablePtr> editorTickables_;

	/**
	 *	Set of custom renderable objects.
	 */
	std::set<EditorRenderablePtr> editorRenderables_;

	bool writeDirtyList();

	Vector3	worldRay_;

	float angleSnaps_;
	Vector3 movementSnaps_;
	Vector3 movementDeltaSnaps_;
	void calculateSnaps();


	volatile bool killingUpdatingFiber_;
	bool settingSelection_;
	LPVOID mainFiber_;
	LPVOID updatingFiber_;

	static void WINAPI backgroundUpdateLoop( PVOID param );
	BWLock::WorldEditordConnection conn_;

	typedef std::pair<Matrix, Terrain::BaseTerrainBlockPtr >	BlockInPlace;
	AVectorNoDestructor< BlockInPlace >	readOnlyTerrainBlocks_;

	typedef std::vector< std::string > StringVector;

	static SimpleMutex pendingMessagesMutex_;
	static StringVector pendingMessages_;
	static void postPendingErrorMessages();

	static SmartPointer<WorldManager> s_instance;

	std::vector<ChunkItemPtr> selectedItems_;

	SmartPointer<WorldEditorCamera> worldEditorCamera_;

	ChunkDirMapping* mapping_;

	// Current chunk that we're counting prim groups for, used for status bar display
	Chunk* currentMonitoredChunk_;
	// Chunk amount of primitive groups in the locator's chunk, used for status bar display
	uint currentPrimGroupCount_;

	// handle the debug messages
	static WorldEditorDebugMessageCallback debugMessageCallback_;

	EditorChunkItemLinkableManager linkerManager_;
	
	SpaceManager* spaceManager_;
	std::string currentSpace_;

	bool changeSpace( const std::string& space, bool reload );
	bool changeSpace( GUI::ItemPtr item );
	bool newSpace( GUI::ItemPtr item );
	bool recentSpace( GUI::ItemPtr item );
	bool setLanguage( GUI::ItemPtr item );
	bool clearUndoRedoHistory( GUI::ItemPtr item );
	bool doReloadAllChunks( GUI::ItemPtr item );
	bool doExit( GUI::ItemPtr item );
	bool doReloadAllTextures( GUI::ItemPtr item );
	bool recalcCurrentChunk( GUI::ItemPtr item );
	unsigned int updateUndo( GUI::ItemPtr item );
	unsigned int updateRedo( GUI::ItemPtr item );
	void updateRecentFile();
	void updateLanguageList();

	bool doExternalEditor( GUI::ItemPtr item );
	unsigned int updateExternalEditor( GUI::ItemPtr item );
	unsigned int updateLanguage( GUI::ItemPtr item );

	bool initPanels();
	bool loadDefaultPanels( GUI::ItemPtr item = 0 );

	virtual std::string get( const std::string& key ) const;
	virtual bool exist( const std::string& key ) const;
	virtual void set( const std::string& key, const std::string& value );

	bool drawMissingTextureLOD(Chunk *chunk, bool markDirty);
	void drawMissingTextureLODs(bool complainIfNotDone, bool doAll, 
			bool markDirty, bool progress = false);

	void setCursor();

	std::vector<std::string> statusMessages_;
	DWORD lastModifyTime_;
	bool drawSelection_;
	std::set< EditorChunkItem* > drawSelectionItems_;

	HCURSOR cursor_;

    bool waitCursor_;
    void showBusyCursor();

	// used to store the state of the last save/quickSave
	bool saveFailed_;
	bool inEscapableProcess_;
	bool warningOnLowMemory_;

	ChunkWatcherPtr chunkWatcher_;
	TerrainUtils::TerrainFormat terrainInfo_;
	bool terrainInfoClean_;
	bool renderDisabled_;

	float timeLastUpdateTexLod_;

	WorldEditorProgressBar *progressBar_;
	std::string currentLanguageName_; 
	std::string currentCountryName_; 

	SimpleMutex changeMutex;
	std::set< Chunk * > pendingChangedChunks_;

	bool isSaving_;

	LONG slowTaskCount_;
	HCURSOR savedCursor_;
	SimpleMutex savedCursorMutex_;
};


bool chunkWritable( Chunk * pChunk, bool bCheckSurroundings = true );

bool chunkWritable( const std::string& identifier, bool bCheckSurroundings = true );


class SelectionOperation : public UndoRedo::Operation
{
public:
	SelectionOperation( const std::vector<ChunkItemPtr>& before, const std::vector<ChunkItemPtr>& after );

private:
	virtual void undo();
	virtual bool iseq( const UndoRedo::Operation & oth ) const;

	std::vector<ChunkItemPtr> before_;
	std::vector<ChunkItemPtr> after_;
};


#endif // WORLD_MANGER_HPP
