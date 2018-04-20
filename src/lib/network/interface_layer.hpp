/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef INTERFACE_LAYER_HPP
#define INTERFACE_LAYER_HPP


#include "network/mercury.hpp"

/**
 *	This class defines a simple exception handling
 *	utility class for the network interface layer.
 */
class InterfaceLayerError
{
public:
	/// @todo Comment
	InterfaceLayerError(int why) : reason(why) {}
	/// @todo Comment
	int	reason;
};







template <class SERV> class IFHandler : public Mercury::InputMessageHandler
{
public:
	IFHandler() : pInst_(NULL), pNub_(NULL)
	{
	}

	void setInstance(SERV & inst)
	{
		pInst_ = &inst;
	}

	void setNub(Mercury::Nub & nub)
	{
		pNub_ = &nub;
	}
	/*
	virtual void handleMessage(const Mercury::Address & source,
		UnpackedMessageHeader & header,
		BinaryIStream & data) = 0;
	*/
protected:
	SERV			*pInst_;
	Mercury::Nub	*pNub_;
};


/* Way to get rid of your ampersand... */
template <class C> struct constructor_type { typedef C type; };
template <class C> struct constructor_type<C &> { typedef C type; };

/* Some handy macros */

#define ARGBODY(N)					\
typename constructor_type<ARG##N>::type	arg##N;	data >> arg##N;

#define REPLYBODY					\
	Mercury::Bundle		b;			\
	b.startReply(header.replyID);	\
	b << r;							\
	pNub_->send(source,b);


/* Default dispatcher for 0 argument functions */
template <class SERV, class RET>
	class IFHandler0 :
		public IFHandler<SERV>
{
public:
	typedef		RET (SERV::*SubFnType)();
	IFHandler0(SubFnType subFn)	{ subFn_ = subFn; }

	virtual void handleMessage(const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & /*data*/)
	{
		RET r = (pInst_->*subFn_)();
		REPLYBODY
	}

private:
	SubFnType		subFn_;
};

/* Void dispatcher for 0 argument functions */
template <class SERV>
	class IFHandler0<SERV,void> :
		public IFHandler<SERV>
{
public:
	typedef		void (SERV::*SubFnType)();
	IFHandler0(SubFnType subFn)	{ subFn_ = subFn; }

	virtual void handleMessage(const Mercury::Address & /*source*/,
		Mercury::UnpackedMessageHeader & /*header*/,
		BinaryIStream & /*data*/)
	{
		(pInst_->*subFn_)();
	}

private:
	SubFnType		subFn_;
};

/* Default dispatcher for 1 argument functions */
template <class SERV, class RET, class ARG1>
	class IFHandler1 :
		public IFHandler<SERV>
{
public:
	typedef		RET (SERV::*SubFnType)(ARG1);
	IFHandler1(SubFnType subFn)	{ subFn_ = subFn; }

	virtual void handleMessage(const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data)
	{
		ARGBODY(1)
		RET r = (pInst_->*subFn_)(arg1);
		REPLYBODY
	}

private:
	SubFnType		subFn_;
};

/* Void dispatcher for 1 argument functions */
template <class SERV, class ARG1>
	class IFHandler1<SERV,void,ARG1> :
		public IFHandler<SERV>
{
public:
	typedef		void (SERV::*SubFnType)(ARG1);
	IFHandler1(SubFnType subFn)	{ subFn_ = subFn; }

	virtual void handleMessage(const Mercury::Address & /*source*/,
		Mercury::UnpackedMessageHeader & /*header*/,
		BinaryIStream & data)
	{
		ARGBODY(1)
		(pInst_->*subFn_)(arg1);
	}

private:
	SubFnType		subFn_;
};

/* Default dispatcher for 2 argument functions */
template <class SERV, class RET, class ARG1, class ARG2>
	class IFHandler2 :
		public IFHandler<SERV>
{
public:
	typedef		RET (SERV::*SubFnType)(ARG1,ARG2);
	IFHandler2(SubFnType subFn)	{ subFn_ = subFn; }

	virtual void handleMessage(const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data)
	{
		ARGBODY(1)
		ARGBODY(2)
		RET r = (pInst_->*subFn_)(arg1,arg2);
		REPLYBODY
	}

private:
	SubFnType		subFn_;
};

/* Void dispatcher for 2 argument functions */
template <class SERV, class ARG1, class ARG2>
	class IFHandler2<SERV,void,ARG1,ARG2> :
		public IFHandler<SERV>
{
public:
	typedef		void (SERV::*SubFnType)(ARG1,ARG2);
	IFHandler2(SubFnType subFn)	{ subFn_ = subFn; }

	virtual void handleMessage(const Mercury::Address & /*source*/,
		Mercury::UnpackedMessageHeader & /*header*/,
		BinaryIStream & data)
	{
		ARGBODY(1)
		ARGBODY(2)
		(pInst_->*subFn_)(arg1,arg2);
	}

private:
	SubFnType		subFn_;
};

/* Default dispatcher for 3 argument functions */
template <class SERV, class RET, class ARG1, class ARG2, class ARG3>
	class IFHandler3 :
		public IFHandler<SERV>
{
public:
	typedef		RET (SERV::*SubFnType)(ARG1,ARG2,ARG3);
	IFHandler3(SubFnType subFn)	{ subFn_ = subFn; }

	virtual void handleMessage(const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data)
	{
		ARGBODY(1)
		ARGBODY(2)
		ARGBODY(3)
		RET r = (pInst_->*subFn_)(arg1,arg2,arg3);
		REPLYBODY
	}

private:
	SubFnType		subFn_;
};

/* Void dispatcher for 3 argument functions */
template <class SERV, class ARG1, class ARG2, class ARG3>
	class IFHandler3<SERV,void,ARG1,ARG2,ARG3> :
		public IFHandler<SERV>
{
public:
	typedef		void (SERV::*SubFnType)(ARG1,ARG2,ARG3);
	IFHandler3(SubFnType subFn)	{ subFn_ = subFn; }

	virtual void handleMessage(const Mercury::Address & /*source*/,
		Mercury::UnpackedMessageHeader & /*header*/,
		BinaryIStream & data)
	{
		ARGBODY(1)
		ARGBODY(2)
		ARGBODY(3)
		(pInst_->*subFn_)(arg1,arg2,arg3);
	}

private:
	SubFnType		subFn_;
};


/* Default dispatcher for 4 argument functions */
template <class SERV, class RET, class ARG1, class ARG2, class ARG3, class ARG4>
	class IFHandler4 :
		public IFHandler<SERV>
{
public:
	typedef		RET (SERV::*SubFnType)(ARG1,ARG2,ARG3,ARG4);
	IFHandler4(SubFnType subFn)	{ subFn_ = subFn; }

	virtual void handleMessage(const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data)
	{
		ARGBODY(1)
		ARGBODY(2)
		ARGBODY(3)
		ARGBODY(4)
		RET r = (pInst_->*subFn_)(arg1,arg2,arg3,arg4);
		REPLYBODY
	}

private:
	SubFnType		subFn_;
};

/* Void dispatcher for 4 argument functions */
template <class SERV, class ARG1, class ARG2, class ARG3, class ARG4>
	class IFHandler4<SERV,void,ARG1,ARG2,ARG3,ARG4> :
		public IFHandler<SERV>
{
public:
	typedef		void (SERV::*SubFnType)(ARG1,ARG2,ARG3,ARG4);
	IFHandler4(SubFnType subFn)	{ subFn_ = subFn; }

	virtual void handleMessage(const Mercury::Address & /*source*/,
		Mercury::UnpackedMessageHeader & /*header*/,
		BinaryIStream & data)
	{
		ARGBODY(1)
		ARGBODY(2)
		ARGBODY(3)
		ARGBODY(4)
		(pInst_->*subFn_)(arg1,arg2,arg3,arg4);
	}

private:
	SubFnType		subFn_;
};



/*
	I would love to put these in interface_layer.cpp, but I can't
	get the syntax for out-of-class definitions right (and it
	doesn't make much sense for templates ... especially if
	they're in different .o's)
*/
/**
 *  Mercury magic used in interface_layer_macros.hpp for declaring interfaces.
 */
template <class SERV> class InterfaceLayer
{
public:
	InterfaceLayer( const char * name ) :
		sumServer_(false),
		next_(0),
		name_(name)
	{}

	~InterfaceLayer()
	{
		for(uint i=0;i<elts_.size();i++)
			delete elts_[i];
		elts_.clear();
	}

	void setInstance( SERV & inst )
	{
		for(uint i=0;i<elts_.size();i++)
			elts_[i]->dispatcher->setInstance(inst);
	}

	int registerWithNub(Mercury::Nub & nub, int id, bool publicise = true)
	{
		for(uint i=0;i<elts_.size();i++)
			elts_[i]->dispatcher->setNub(nub);

		iterator	b = begin(), e = end();
		return nub.serveInterface(b,e,NULL,name_,id,publicise);
	}

	Mercury::InterfaceElement & add(IFHandler<SERV> * newDispo = NULL)
	{
		LayerElt	*newLE = new LayerElt;

		newLE->ie.identifier = next_++;
		newLE->ie.lengthStyle = Mercury::VARIABLE_LENGTH_MESSAGE;
		newLE->ie.lengthParam = 2;
		newLE->dispatcher = newDispo;

		if (newDispo!=NULL) sumServer_ = true;

		elts_.push_back(newLE);

		return newLE->ie;
	}


	/**
	 *	This nested class implements an iterator of InterfaceElement items.
	 */
	class iterator : public Mercury::InterfaceIterator
	{
	public:
		iterator(InterfaceLayer *from, unsigned char index) :
			from_(from),
			index_(index)
		{
		}

		virtual Mercury::InputMessageHandler * handler()
			{ return (*from_).elts_[index_]->dispatcher; }

		virtual const Mercury::InterfaceElement & operator*()
			{ return (*from_).elts_[index_]->ie; }
		virtual void operator++(int)
			{ index_++; }
		virtual bool operator==(const Mercury::InterfaceIterator &i) const
			{ return ((iterator*)&i)->index_ == index_; }
		virtual bool operator!=(const Mercury::InterfaceIterator &i) const
			{ return ((iterator*)&i)->index_ != index_; }
	private:
		InterfaceLayer	* from_;
		unsigned char index_;
	};
	friend class iterator;

	iterator begin()
		{ return iterator(this,0); }
	iterator end()
		{ return iterator(this,next_); }
private:
	struct LayerElt
	{
		Mercury::InterfaceElement	ie;
		IFHandler<SERV>				*dispatcher;
	};

	std::vector<LayerElt*>	elts_;

	bool				sumServer_;
	unsigned char		next_;
	const char			* name_;
};





#endif //INTERFACE_LAYER_HPP
