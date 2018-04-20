/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


#ifndef INTERFACE_LAYER_MACROS_HPP
	#define INTERFACE_LAYER_MACROS_HPP

	#include "network/interface_layer.hpp"
#endif // INTERFACE_LAYER_MACROS_HPP

#undef LAYER_BEGIN
#undef LAYER_END

#undef LAYER_FUNCTION_0
#undef LAYER_FUNCTION_0V
#undef LAYER_FUNCTION_1
#undef LAYER_FUNCTION_1V
#undef LAYER_FUNCTION_2
#undef LAYER_FUNCTION_2V
#undef LAYER_FUNCTION_3
#undef LAYER_FUNCTION_3V
#undef LAYER_FUNCTION_4
#undef LAYER_FUNCTION_4V


#ifndef DO_SOURCE
	// Header file stuff
	#define LAYER_BEGIN(NAME) \
	class SERVER_TYPE; \
	namespace NAME { \
		extern InterfaceLayer<SERVER_TYPE> gLayer; \
		int registerWithNub(Mercury::Nub & nub, int id, \
			bool publicise = true); \
		\
		class IF { \
		public: \
			IF( const Mercury::Address & addr, Mercury::Nub & nub );

	#define LAYER_END() \
		const Mercury::Address & __addr() const { return addr_; } \
		private: \
			Mercury::Address	addr_; \
			Mercury::Nub		& nub_; \
		}; \
	}

	#undef LAYER_FUNCTION_COMMON
	#define LAYER_FUNCTION_COMMON(RET,NAME)	\
		static Mercury::InterfaceElement & NAME##__IE; \
		constructor_type<RET>::type NAME

	#define LAYER_FUNCTION_0V LAYER_FUNCTION_0
	#define LAYER_FUNCTION_1V LAYER_FUNCTION_1
	#define LAYER_FUNCTION_2V LAYER_FUNCTION_2
	#define LAYER_FUNCTION_3V LAYER_FUNCTION_3
	#define LAYER_FUNCTION_4V LAYER_FUNCTION_4

	#define LAYER_FUNCTION_0(RET,NAME)	\
		LAYER_FUNCTION_COMMON(RET,NAME) (); \

	#define LAYER_FUNCTION_1(RET,NAME,ARG1)	\
		LAYER_FUNCTION_COMMON(RET,NAME) (const ARG1 arg1);

	#define LAYER_FUNCTION_2(RET,NAME,ARG1,ARG2)	\
		LAYER_FUNCTION_COMMON(RET,NAME) (const ARG1 arg1, const ARG2 arg2);

	#define LAYER_FUNCTION_3(RET,NAME,ARG1,ARG2,ARG3)	\
		LAYER_FUNCTION_COMMON(RET,NAME) \
			(const ARG1 arg1, const ARG2 arg2, const ARG3 arg3);

	#define LAYER_FUNCTION_4(RET,NAME,ARG1,ARG2,ARG3,ARG4)	\
		LAYER_FUNCTION_COMMON(RET,NAME) \
			(const ARG1 arg1, const ARG2 arg2, const ARG3 arg3, \
				const ARG4 arg4);

#else // DO_SOURCE
	// Source file stuff

	// begin and end
	#define LAYER_BEGIN(NAME) \
	LAYER_BEGIN_EXTRA_A \
	namespace NAME { \
		InterfaceLayer<SERVER_TYPE>		gLayer(#NAME); \
		\
		int registerWithNub LAYER_BEGIN_EXTRA_B \
		\
		IF::IF( const Mercury::Address & addr, Mercury::Nub & nub ) : \
			addr_(addr), nub_(nub) {}


	#define LAYER_END() }

	// helper definitions
	#undef LAYER_FUNCTION_COMMON_A
	#define LAYER_FUNCTION_COMMON_A(RET,NAME) \
	Mercury::InterfaceElement & IF::NAME##__IE = \
		gLayer.add(NULLIFY_FOR_CLIENT(&NAME##__Handler)); \
	constructor_type<RET>::type IF::NAME

	#undef LAYER_FUNCTION_COMMON_B
	#define LAYER_FUNCTION_COMMON_B(RET,NAME) \
		Mercury::BlockingReplyHandler<constructor_type<RET>::type>	\
			hand(nub_); \
		Mercury::Bundle		b; \
		b.startRequest(IF::NAME##__IE,&hand);

	#undef LAYER_FUNCTION_COMMON_C
	#define LAYER_FUNCTION_COMMON_C \
		hand.await(); \
		if(hand.err()) throw(InterfaceLayerError(hand.err())); \
		return hand.get();

	#undef LAYER_FUNCTION_COMMON_BV
	#define LAYER_FUNCTION_COMMON_BV(NAME) \
		Mercury::Bundle		b; \
		b.startMessage(IF::NAME##__IE);

	#undef LAYER_FUNCTION_SEND
	#define LAYER_FUNCTION_SEND \
		nub_.send(addr_,b);


	// layer function definitions

	#define LAYER_FUNCTION_0(RET,NAME) \
	LAYER_FUNCTION_EXTRA_0(RET,NAME) \
	LAYER_FUNCTION_COMMON_A(RET,NAME) () \
	{ \
		LAYER_FUNCTION_COMMON_B(RET,NAME) \
		LAYER_FUNCTION_SEND \
		LAYER_FUNCTION_COMMON_C \
	}

	#define LAYER_FUNCTION_0V(RET,NAME) \
	LAYER_FUNCTION_EXTRA_0(RET,NAME) \
	LAYER_FUNCTION_COMMON_A(RET,NAME) () \
	{ \
		LAYER_FUNCTION_COMMON_BV(NAME) \
		LAYER_FUNCTION_SEND \
	}

	#define LAYER_FUNCTION_1(RET,NAME,ARG1) \
	LAYER_FUNCTION_EXTRA_1(RET,NAME,ARG1) \
	LAYER_FUNCTION_COMMON_A(RET,NAME) (const ARG1 arg1) \
	{ \
		LAYER_FUNCTION_COMMON_B(RET,NAME) \
		b << arg1; \
		LAYER_FUNCTION_SEND \
		LAYER_FUNCTION_COMMON_C \
	}

	#define LAYER_FUNCTION_1V(RET,NAME,ARG1) \
	LAYER_FUNCTION_EXTRA_1(RET,NAME,ARG1) \
	LAYER_FUNCTION_COMMON_A(RET,NAME) (const ARG1 arg1) \
	{ \
		LAYER_FUNCTION_COMMON_BV(NAME) \
		b << arg1; \
		LAYER_FUNCTION_SEND \
	}

	#define LAYER_FUNCTION_2(RET,NAME,ARG1,ARG2) \
	LAYER_FUNCTION_EXTRA_2(RET,NAME,ARG1,ARG2) \
	LAYER_FUNCTION_COMMON_A(RET,NAME) (const ARG1 arg1, const ARG2 arg2) \
	{ \
		LAYER_FUNCTION_COMMON_B(RET,NAME) \
		b << arg1; \
		b << arg2; \
		LAYER_FUNCTION_SEND \
		LAYER_FUNCTION_COMMON_C \
	}

	#define LAYER_FUNCTION_2V(RET,NAME,ARG1,ARG2) \
	LAYER_FUNCTION_EXTRA_2(RET,NAME,ARG1,ARG2) \
	LAYER_FUNCTION_COMMON_A(RET,NAME) (const ARG1 arg1, const ARG2 arg2) \
	{ \
		LAYER_FUNCTION_COMMON_BV(NAME) \
		b << arg1; \
		b << arg2; \
		LAYER_FUNCTION_SEND \
	}

	#define LAYER_FUNCTION_3(RET,NAME,ARG1,ARG2,ARG3) \
	LAYER_FUNCTION_EXTRA_3(RET,NAME,ARG1,ARG2,ARG3) \
	LAYER_FUNCTION_COMMON_A(RET,NAME) \
		(const ARG1 arg1, const ARG2 arg2, const ARG3 arg3) \
	{ \
		LAYER_FUNCTION_COMMON_B(RET,NAME) \
		b << arg1; \
		b << arg2; \
		b << arg3; \
		LAYER_FUNCTION_SEND \
		LAYER_FUNCTION_COMMON_C \
	}

	#define LAYER_FUNCTION_3V(RET,NAME,ARG1,ARG2,ARG3) \
	LAYER_FUNCTION_EXTRA_3(RET,NAME,ARG1,ARG2,ARG3) \
	LAYER_FUNCTION_COMMON_A(RET,NAME) \
		(const ARG1 arg1, const ARG2 arg2, const ARG3 arg3) \
	{ \
		LAYER_FUNCTION_COMMON_BV(NAME) \
		b << arg1; \
		b << arg2; \
		b << arg3; \
		LAYER_FUNCTION_SEND \
	}

	#define LAYER_FUNCTION_4(RET,NAME,ARG1,ARG2,ARG3,ARG4) \
	LAYER_FUNCTION_EXTRA_4(RET,NAME,ARG1,ARG2,ARG3,ARG4) \
	LAYER_FUNCTION_COMMON_A(RET,NAME) \
		(const ARG1 arg1, const ARG2 arg2, const ARG3 arg3, const ARG4 arg4) \
	{ \
		LAYER_FUNCTION_COMMON_B(RET,NAME) \
		b << arg1; \
		b << arg2; \
		b << arg3; \
		b << arg4; \
		LAYER_FUNCTION_SEND \
		LAYER_FUNCTION_COMMON_C \
	}

	#define LAYER_FUNCTION_4V(RET,NAME,ARG1,ARG2,ARG3,ARG4) \
	LAYER_FUNCTION_EXTRA_4(RET,NAME,ARG1,ARG2,ARG3,ARG4) \
	LAYER_FUNCTION_COMMON_A(RET,NAME) \
		(const ARG1 arg1, const ARG2 arg2, const ARG3 arg3, const ARG4 arg4) \
	{ \
		LAYER_FUNCTION_COMMON_BV(NAME) \
		b << arg1; \
		b << arg2; \
		b << arg3; \
		b << arg4; \
		LAYER_FUNCTION_SEND \
	}


	// client/server dependent definitions
	#ifndef DO_SERVER
		#undef LAYER_BEGIN_EXTRA_A
		#define LAYER_BEGIN_EXTRA_A class SERVER_TYPE {};

		#undef LAYER_BEGIN_EXTRA_B
		#define LAYER_BEGIN_EXTRA_B \
			(Mercury::Nub &, int, bool) \
			{ return Mercury::REASON_CORRUPTED_PACKET; }

		#undef NULLIFY_FOR_CLIENT
		#define NULLIFY_FOR_CLIENT(X) NULL

		#undef LAYER_FUNCTION_EXTRA_0
		#define LAYER_FUNCTION_EXTRA_0(RET,NAME)

		#undef LAYER_FUNCTION_EXTRA_1
		#define LAYER_FUNCTION_EXTRA_1(RET,NAME,A1)

		#undef LAYER_FUNCTION_EXTRA_2
		#define LAYER_FUNCTION_EXTRA_2(RET,NAME,A1,A2)

		#undef LAYER_FUNCTION_EXTRA_3
		#define LAYER_FUNCTION_EXTRA_3(RET,NAME,A1,A2,A3)

		#undef LAYER_FUNCTION_EXTRA_4
		#define LAYER_FUNCTION_EXTRA_4(RET,NAME,A1,A2,A3,A4)

	#else
		#undef LAYER_BEGIN_EXTRA_A
		#define LAYER_BEGIN_EXTRA_A

		#undef LAYER_BEGIN_EXTRA_B
		#define LAYER_BEGIN_EXTRA_B \
			(Mercury::Nub & nub, int id, bool publicise) \
			{ return gLayer.registerWithNub(nub,id,publicise); }

		#undef NULLIFY_FOR_CLIENT
		#define NULLIFY_FOR_CLIENT(X) X

		#undef LAYER_FUNCTION_EXTRA_0
		#define LAYER_FUNCTION_EXTRA_0(RET,NAME) \
		IFHandler0<SERVER_TYPE,RET>	\
			NAME##__Handler(&SERVER_TYPE::NAME);

		#undef LAYER_FUNCTION_EXTRA_1
		#define LAYER_FUNCTION_EXTRA_1(RET,NAME,A1) \
		IFHandler1<SERVER_TYPE,RET,A1> \
			NAME##__Handler(&SERVER_TYPE::NAME);

		#undef LAYER_FUNCTION_EXTRA_2
		#define LAYER_FUNCTION_EXTRA_2(RET,NAME,A1,A2) \
		IFHandler2<SERVER_TYPE,RET,A1,A2> \
			NAME##__Handler(&SERVER_TYPE::NAME);

		#undef LAYER_FUNCTION_EXTRA_3
		#define LAYER_FUNCTION_EXTRA_3(RET,NAME,A1,A2,A3) \
		IFHandler3<SERVER_TYPE,RET,A1,A2,A3> \
			NAME##__Handler(&SERVER_TYPE::NAME);

		#undef LAYER_FUNCTION_EXTRA_4
		#define LAYER_FUNCTION_EXTRA_4(RET,NAME,A1,A2,A3,A4) \
		IFHandler4<SERVER_TYPE,RET,A1,A2,A3,A4> \
			NAME##__Handler(&SERVER_TYPE::NAME);

	#endif

#endif

#undef DO_SOURCE



// Client: want to be able to have a coolFace class and call
// coolFunction(int a, int b) on it

// Server: want to be able to do some simple init and have things
// called automatically. Specify a class type and instance and
// then write functions in the class with the same names/return
// types as in the header.

// Header:

/*
namespace coolFace
{
	class SERV;
	extern InterfaceLayer<SERV>	gLayer;
	int registerWithNub(Mercury::Nub & nub, int id, bool publicise = true);

	class IF
	{
	public:
		extern IF( const Mercury::Address & addr,
			Mercury::Nub & nub );

		extern static InterfaceElement & coolProcedure__IE;
		void coolProcedure( int arg1, int arg2 );

		extern static InterfaceElement & coolFunction__IE;
		int coolFunction( std::string arg1 );

	private:
		Mercury::Address	addr_;
		Mercury::Nub		nub_;
	};
};
*/


// Client source:
/*
namespace coolFace
{
	class SERV { };
	InterfaceLayer<SERV>		gLayer("coolFace");

	int registerWithNub(Mercury::Nub & nub, int id, bool publicise)
	{
		return Mercury::REASON_CORRUPTED_PACKET;
	}

	IF::IF( const Mercury::Address & addr, Mercury::Nub & nub ) :
		addr_(addr), nub_(nub)
	{
	}

	IF::coolProcedure__IE = gLayer.add(NULL);
	void IF::coolProcedure( const int arg1, const int arg2 )
	{
		Mercury::Bundle		b;
		b.startMessage(coolProcedure__IE);
		b << arg1;
		b << arg2;
		nub_.send(addr_,b);
	}

	IF::coolFunction__IE = gLayer.add(NULL);
	int IF::coolFunction( const std::string & arg1 )
	{
		Mercury::BlockingReplyHandler<int>	hand(nub_);

		Mercury::Bundle		b;
		b.startRequest(coolFunction__IE,hand);
		b << arg1;
		nub_.send(addr_,b);

		hand.await();
		if(hand.err()) throw(InterfaceLayerError(hand.err()));
		return hand.get();
	}
}
*/


// Server source:
// NOTE: header files need #define SERVER_TYPE [sometype]
/*
// same as client source except the following:

// start-=
class SERV { };
// and registerWithNub becomes
gLayer.registerWithNub(nub,id,publicise);

// fn definition headers change from
IF::coolFunction__IE = gClientLayer.add(NULL);
// to
IFHandler1<SERV,int,int>	coolFunction__Handler(&SERV::coolFunction);
IF::coolFunction__IE = gClientLayer.add(&coolFunction__Handler);
*/









/*


//start+=
template <class RET> LIFHandler0:
	public IFHandler0<SERV,RET> {};
template <class RET, class ARG1> LIFHandler1:
	public IFHandler1<SERV,RET,ARG1> {};
template <class RET, class ARG1, class ARG2> LIFHandler2:
	public IFHandler2<SERV,RET,ARG1,ARG2> {};
template <class RET, class ARG1, class ARG2, class ARG3> LIFHandler3:
	public IFHandler3<SERV,RET,ARG1,ARG2,ARG3> {};
template <class RET, class ARG1, class ARG2, class ARG3, class ARG4> LIFHandler4:
	public IFHandler4<SERV,RET,ARG1,ARG2,ARG3,ARG4> {};

*/
