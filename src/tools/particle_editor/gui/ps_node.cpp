/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "ps_node.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/string_provider.hpp"

using namespace std;


namespace
{
    string const PARTICLE_SYS_EXT = "xml";
}


MetaNode::MetaNode(string const &dir, string const &filename) :
    TreeNode(),
    m_metaParticleSystem(NULL),
    m_directory(dir),
    m_createdChildren(false),
	m_readOnly(false)
{
    string label;
    label = BWResource::removeExtension(filename);
    label = BWResource::getFilename(label);
    SetLabel(label.c_str());
    m_lastName = label;
}


/*virtual*/ MetaNode::~MetaNode()
{
    if (m_metaParticleSystem != NULL)
    {
        m_metaParticleSystem->detach();
        m_metaParticleSystem = NULL;
    }
}


/*virtual*/ void MetaNode::SetLabel(string const &label)
{
    // Rename the underlying file:
    string oldFilename = GetFilename();
    if (!oldFilename.empty())
    {
        string oldFullPath = fullpath(GetFilename());
        string newFullPath = fullpath(label + '.' + PARTICLE_SYS_EXT);
        if (oldFullPath != newFullPath)
            rename(oldFullPath.c_str(), newFullPath.c_str());
    }

    // Change the actual label:
    TreeNode::SetLabel(label);
}


/*virtual*/ bool MetaNode::CanEditLabel() const
{
    EnsureLoaded();
    return m_metaParticleSystem != NULL;
}


/*virtual*/ DROPEFFECT MetaNode::CanDragDrop() const
{
    EnsureLoaded();
    if (m_metaParticleSystem != NULL)
      return DROPEFFECT_COPY;
    else
        return DROPEFFECT_NONE;
}


MetaParticleSystemPtr MetaNode::GetMetaParticleSystem() const
{
    return m_metaParticleSystem;
}


void MetaNode::SetMetaParticleSystem(MetaParticleSystemPtr system)
{
    m_metaParticleSystem = system;
}

void MetaNode::SetReadOnly( bool readOnly )
{
	m_readOnly = readOnly;
}

bool MetaNode::IsReadOnly() const
{
	return m_readOnly;
}

void MetaNode::EnsureLoaded() const
{
    if (m_metaParticleSystem == NULL)
    {
        m_metaParticleSystem = new MetaParticleSystem();
        bool ok = m_metaParticleSystem->load(GetFilename(), m_directory);
        if (!ok)
        {
            m_metaParticleSystem = NULL;
        }
		else
		{
			string fullPath = BWResource::resolveFilename( m_directory + GetFilename() );
			DWORD att = GetFileAttributes( fullPath.c_str() );
			if ( att & FILE_ATTRIBUTE_READONLY )
			{
				::MessageBox( AfxGetMainWnd()->GetSafeHwnd(), 
					L("`RCS_IDS_READONLY", fullPath ),
					L("`RCS_IDS_READONLYTITLE"),
					MB_ICONEXCLAMATION | MB_OK );
				m_readOnly = true;
			}
		}
    }
}


string MetaNode::GetFilename() const
{
    return GetFilename(GetLabel());
}


/*static*/ std::string MetaNode::GetFilename(std::string const &file)
{
    return file + '.' + PARTICLE_SYS_EXT;
}


string const &MetaNode::GetDirectory() const
{
    return m_directory;
}


string MetaNode::GetFullpath() const
{
    return fullpath(GetFilename());
}


bool MetaNode::DeleteFile()
{
    string file = GetFullpath();
    return unlink(file.c_str()) == 0;
}


void MetaNode::FlagChildrenReady()
{
    m_createdChildren = true;
}


void MetaNode::onSave()
{
    m_lastName = GetLabel();
}


void MetaNode::onNotSave()
{
    if (m_lastName != GetLabel())
    {
        std::string name = m_lastName;        
        SetLabel(name);
        m_lastName = name;
    }
}


void MetaNode::onRename()
{
    //m_lastName = GetLabel();
}


/*virtual*/ TreeNode *
MetaNode::Serialise
(
    DataSectionPtr  data, 
    bool            load
)
{
    TreeNode *result = TreeNode::Serialise(data, load);
    if (load)
    {
        m_lastName = data->readString("lastSaveName", GetLabel());
    }
    else
    {
        data->writeString("lastSaveName", m_lastName);
    }
    return result;
}


/*static*/ bool MetaNode::IsMetaParticleFile(string const &filename)
{
    string extension = BWResource::getExtension(filename);
    return stricmp(extension.c_str(), PARTICLE_SYS_EXT.c_str()) == 0;
}


/*virtual*/ void MetaNode::OnExpand()
{
	TreeControl *tree = GetTreeControl();
	tree->SetSelectedNode(this);
    // If already expanded do nothing:
    if (m_createdChildren)
        return;

    EnsureLoaded();

    if (m_metaParticleSystem == NULL)
        return;

    

    // Add the particle systems:
    vector<ParticleSystemPtr> &partileSyss = m_metaParticleSystem->systemSet();
    for (size_t i = 0; i < partileSyss.size(); ++i)
    {
        ParticleSystemPtr partSys = partileSyss[i];
        PSNode *psnode = (PSNode *)tree->AddNode(new PSNode(partSys), this);
		tree->SetCheck( *psnode, psnode->getParticleSystem()->enabled() ? BST_CHECKED : BST_UNCHECKED );
        psnode->addChildren();
    }
    m_createdChildren = true;
}


/*virtual*/ bool MetaNode::IsVirtualNode() const
{
    return true;
}


/*virtual*/ bool MetaNode::DeleteNeedsConfirm() const
{
    return true;
}


/*virtual*/ void MetaNode::OnEditLabel(string const &/*newLabel*/)
{
    // The parent needs to receive the notification, verify the model and
    // make the change via RenameFile.
}


string MetaNode::fullpath(string const &filename) const
{
    string fullpath = m_directory + filename;
    if (!fullpath.empty() && (fullpath[0] == '/' || fullpath[0] == '\\'))
        fullpath.erase(fullpath.begin());
    fullpath = BWResource::resolveFilename(fullpath);
    return fullpath;
}


/*explicit*/ PSNode::PSNode(ParticleSystemPtr ps) :
    TreeNode(),
    m_particleSystem(ps)
{
    if (ps != NULL)
        SetLabel(m_particleSystem->name().c_str());
}


/*virtual*/ void PSNode::SetLabel(string const &label)
{
    m_particleSystem->name(label);
    TreeNode::SetLabel(label);
}


/*virtual*/ DROPEFFECT PSNode::CanDragDrop() const
{
    return DROPEFFECT_COPY;
}


void PSNode::addChildren()
{
    TreeControl *tree = GetTreeControl();
	TreeNode *newNode;
    // Add the system properties node:
    newNode = tree->AddNode
    (
        new ActionNode(NULL, L("PARTICLEEDITOR/GUI/SYSTEM_PROP"), ActionNode::AT_SYS_PROP), 
        this
    );
	tree->SetItemState( *newNode, 0, TVIS_STATEIMAGEMASK ); 
    // Add the renderer properties nodes:
    newNode = tree->AddNode
    (
        new ActionNode(NULL, L("PARTICLEEDITOR/GUI/RENDERER_PROP"), ActionNode::AT_REND_PROP), 
        this
    ); 
	tree->SetItemState( *newNode, 0, TVIS_STATEIMAGEMASK ); 
    // Add the action nodes:
    vector<ParticleSystemActionPtr> &actions = m_particleSystem->actionSet();
    typedef map<string, int> NameCountMap;
    NameCountMap names;
    for (size_t j = 0; j < actions.size(); ++j)
    {
        ParticleSystemActionPtr psa = actions[j];
        // Generate a name for the action if it doesn't have one:
        string name = psa->name();
        if (name.empty())
        {
            NameCountMap::iterator it = names.find(psa->nameID());
            if (it == names.end())
                names[psa->nameID()] = 0;
            char buffer[512];
            snprintf
            (
                buffer, 
                sizeof(buffer)/sizeof(char) - 1, 
                "%s %i",
                psa->nameID().c_str(),
                ++names[psa->nameID()]
            );
			buffer[511] = '\0';
            psa->name(buffer);
            name = buffer;
        }
        newNode = tree->AddNode(new ActionNode(psa, name), this);   
		ActionNode *actionNode = dynamic_cast<ActionNode*>(newNode);
		if ( actionNode != NULL )
		{
			tree->SetCheck( *actionNode, actionNode->getAction()->enabled() ? BST_CHECKED : BST_UNCHECKED );
		}
    }
}


ParticleSystemPtr PSNode::getParticleSystem() const
{
    return m_particleSystem;
}


/*virtual*/ bool PSNode::DeleteNeedsConfirm() const
{
    return true;
}


ActionNode::ActionNode
(
    ParticleSystemActionPtr action, 
    string                  const &name,
    ActionType              actionType  /*= AT_ACTION*/
) :
    TreeNode(),
    m_action(action),
    m_actionType(actionType)
{
    SetLabel(name.c_str());
}


/*virtual*/ DROPEFFECT ActionNode::CanDragDrop() const
{
    return DROPEFFECT_COPY;
}


ParticleSystemActionPtr ActionNode::getAction()
{
    return m_action;
}


ActionNode::ActionType ActionNode::getActionType()
{
    return m_actionType;
}


 /*virtual*/ bool ActionNode::CanEditLabel() const
{
    return false;
}
