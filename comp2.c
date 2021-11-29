/*--------------------------------------------------------------------------*/
/*                                                                          */
/*       comp2.c      											            */
/*                       													*/
/*       Rory Brennan 18237606												*/
/*                                                                          */
/*   <Program>       :==  "PROGRAM" <Identifier> ";" [<Declarations>]       */
/*                        {<ProcDeclaration>} <Block> "."              (1) 	*/
/*   <Declarations>  :==  "VAR" <Variable> { "," <Variable> }          (2)  */
/* 	 <ProcDeclarati> :==  "PROCEDURE" <Identifier> [<ParameterList>]	    */
/*						  ";" [ <Declarations> ] {<ProcDeclaration>]}	    */											
/*						  <Block> ";"								   (3)  */	
/*	 <ParameterList> :==  "(" <FormalParameter> {"," <FormalParameter}      */
/*						  ")"										   (4)	*/
/*   <FormalParamet> :==  [ "REF" ] <Variable>						   (5)  */
/*   <Block>         :==  "BEGIN" { <Statement> ";" } "END"            (6)  */
/*   <Statement>     :==  <SimpleStatement> | <WhileStatement> |            */
/*						  <IfStatement> | <ReadStatement> |                 */
/*						  <WriteStatement>							   (7)  */
/*   <SimpleStateme> :==  <VarOrProcName> <RestOfStatement>            (8)  */
/*   <RestOfStateme> :==  <ProcCallList> | <Assignment> | eps          (9)  */
/*   <ProcCallList>  :==  "(" <ActualParameter> {"," <ActualParameter>}     */
/*						  ")"										   (10) */
/*   <Assignment>	 :==  ":==" <Expression>						   (11) */
/*   <ActualParamet> :==  <Variable> | <Expression>					   (12) */
/*	 <WhileStatemen> :==  "WHILE" <BooleanExpression> "DO" <Block>     (13) */
/*   <IfStatement>   :==  "IF" <BooleanExpression> "THEN" <Block>			*/
/*						  [ "ELSE" <Block> ]                           (14) */
/*   <ReadStatement> :== "READ" "(" <Variable> {"," <Variable> } ")"   (15) */
/*   <WriteStatemen> :== "WRITE" "(" <Expression> { "," <Expression>        */
/*						 } ")"										   (16) */
/*	 <Expression>    :== <CompoundTerm> { <AddOp> <CompoundTerm> }	   (17) */
/* 	 <CompoundTerm>  :== <Term> { <MultOP> <Term> }					   (18) */
/*   <Term>          :== ["-"] <SubTerm>							   (19) */
/*	 <SubTerm>		 :== <Variable> | <IntConst> | "(" <Expression> ")"(20) */
/* 	 <BooleanExpre>  :== <Expression> <RelOp> <Expression>             (21) */
/*   <RelOp>		 :== "=" | "<=" | ">=" | "<" | ">"				   (22) */
/*                                                                          */
/*                                                                          */
/*       Note - <Identifier> and <IntConst> are provided by the scanner     */
/*       as tokens IDENTIFIER and INTCONST respectively.                    */
/*                                                                          */
/*       As <Variable> is just a renaming of <Identifier>, we will omit     */
/*       any explicit implementation of <Variable>, and just use            */
/*       "Accept( IDENTIFIER );"  wherever a <Variable> is needed.          */
/*                                                                          */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "scanner.h"
#include "symbol.h"
#include "strtab.h"
#include "sets.h"
#include "code.h"
#include "line.h"


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  Global variables used by this compiler.                                 */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE FILE *InputFile;           /*  CPL source comes from here.          */
PRIVATE FILE *ListFile;            /*  For nicely-formatted syntax errors.  */
PRIVATE FILE *CodeFile;			   /*  Assembly code file  					*/

PRIVATE TOKEN  CurrentToken;       /*  Parser lookahead token.  Updated by  */
                                   /*  routine Accept (below).  Must be     */
                                   /*  initialised before parser starts.    */

PRIVATE	SET ProgramFS1_aug, ProgramFS2_aug, ProgramFBS, /* Synchronise Sets */
			ProcDecFS1_aug, ProcDecFS2_aug, ProcDecFBS, 
			StatementFS_aug, StatementFBS;

/* Three global variables dec_local, dec_global, varscope are used to 		*/
/* ensure the correct INC and DEC calls for both local and global variables */
/* Probably don't need these for comp2 but best implementation I have       */
PRIVATE int scope, varaddress, dec_local, dec_global, varscope;


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  Function prototypes                                                     */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseProgram( void );
PRIVATE void ParseDeclarations( void );
PRIVATE void ParseProcDeclaration( void );
PRIVATE void ParseParameterList ( SYMBOL *target );
PRIVATE void ParseFormalParameter( SYMBOL *target );
PRIVATE void ParseBlock( void );
PRIVATE void ParseStatement( void );
PRIVATE void ParseSimpleStatement( void );
PRIVATE void ParseRestOfStatement( SYMBOL *target );
PRIVATE void ParseProcCallList( SYMBOL *target );
PRIVATE void ParseAssignment( void );
PRIVATE void ParseActualParameter( int parflag );
PRIVATE void ParseWhileStatement( void );
PRIVATE void ParseIfStatement( void );
PRIVATE void ParseReadStatement( void );
PRIVATE void ParseWriteStatement( void );
PRIVATE void ParseExpression( void );
PRIVATE void ParseCompoundTerm( void );
PRIVATE void ParseTerm( void );
PRIVATE void ParseSubTerm( void );
PRIVATE int ParseBooleanExpression( void );
PRIVATE int ParseRelOp( void );

PRIVATE int  OpenFiles( int argc, char *argv[] );
PRIVATE void Accept( int code );
PRIVATE void Synchronise( SET *F, SET *FB );
PRIVATE void SetupSets( void );
PRIVATE SYMBOL *MakeSymbolTableEntry( int symtype, int *varaddress );
PRIVATE SYMBOL *LookUpSymbol( void );


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  Main: Program entry point.  Sets up parser globals (opens input and     */
/*        output files, initialises current lookahead), then calls          */
/*        "ParseProgram" to start the parse.                                */
/* 		   Calls SetupSets() to assign synchronisation sets					*/
/*		  Initilizes code generator and begins generating.					*/
/*                                                                          */
/*--------------------------------------------------------------------------*/

PUBLIC int main ( int argc, char *argv[] )
{
	SetupSets();
    if ( OpenFiles( argc, argv ) )  {
        InitCharProcessor( InputFile, ListFile );
		InitCodeGenerator( CodeFile );
        CurrentToken = GetToken();
        ParseProgram();
		WriteCodeFile();
        fclose( InputFile );
        fclose( ListFile );
		fclose( CodeFile );
        return  EXIT_SUCCESS;
    }
    else 
        return EXIT_FAILURE;
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  Parser routines: Recursive-descent implementaion of the grammar's       */
/*                   productions.                                           */
/*                                                                          */
/*                                                                          */
/*  ParseProgram implements:                                                */
/*                                                                          */
/*   <Program>       :==  "PROGRAM" <Identifier> ";" [<Declarations>]       */
/*                        {<ProcDeclaration>} <Block> "."   				*/
/*                                                                          */
/*                                                                          */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      Outputs the Dec call using global variable dec_global	*/    
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseProgram( void )
{
	/* Parse inital program identifier */
    Accept( PROGRAM );  
    Accept( IDENTIFIER );
    Accept( SEMICOLON );
	
	/* Synchronise used to recover parsing after error */
	/* Sets are found at end of program */
	Synchronise(&ProgramFS1_aug, &ProgramFBS);
	
	/* Parse/Compile variables if present */
    if ( CurrentToken.code == VAR )  ParseDeclarations();
	
	Synchronise(&ProgramFS2_aug, &ProgramFBS);
	
	/* Checks for one or more procedures */
	while ( CurrentToken.code == PROCEDURE ) {
        ParseProcDeclaration();
		Synchronise(&ProgramFS2_aug, &ProgramFBS);
    }
	
	/* Begin compiling/parsing main block of code */
    ParseBlock();
	
	/* Dec GLOBAL variables at end of program */
	if( dec_global != 0 && scope < 1)
	{
		Emit( I_DEC, dec_global );
		dec_global = 0;
	}
	
	/* Token "." has name ENDOFPROGRAM */
    Accept( ENDOFPROGRAM );     
	Accept( ENDOFINPUT );
	
	/* Emit Halt call for end of program */
	_Emit( I_HALT );
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  ParseDeclarations implements:                                           */
/*                                                                          */
/*       <Declarations>  :==  "VAR" <Variable> { "," <Variable> }           */
/*                                                                          */
/*                                                                          */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseDeclarations( void )
{
	SYMBOL *var;
	int inc = 1;			/* counter of no. of variable declarations */
	
    Accept( VAR );
	varaddress = CurrentCodeAddress();
	
	/* Used to differentiate between local and global variables */
	if( scope >= 1)
	{
		var = MakeSymbolTableEntry( STYPE_LOCALVAR, &varaddress );
		/* This line is used so that the correct FP offset is applied 
		when calling later on */
		var->address = var->address + dec_global;
	}
	else 
	{
		var = MakeSymbolTableEntry( STYPE_VARIABLE, &varaddress );
	}
	
    Accept( IDENTIFIER );   /* <Variable> is just a remaning of IDENTIFIER. */
    /* EBNF repetition operator {...} implemented as a while-loop.          */
    /* Repetition triggered by "," (i.e, COMMA) in lookahead.               */

    while ( CurrentToken.code == COMMA ) {
        Accept( COMMA );
		varaddress = CurrentCodeAddress();
		
		/* Used to differentiate between local and global variables */
		if( scope >= 1)
		{
			var = MakeSymbolTableEntry( STYPE_LOCALVAR, &varaddress );
			/* Increment address for each new variable */
			var->address++;			
		}
		else {
			var = MakeSymbolTableEntry( STYPE_VARIABLE, &varaddress );
			var->address++;	
		}
        Accept( IDENTIFIER );
		
		/* Increment inc for Inc call later on */
		inc++;
    }
	
    Accept( SEMICOLON );
	
	/* varscope is used to ensure DEC call is in correct scope */
	/* This piece of code stops the INC call being made to early before */
	/* going through all procedures */
	if( scope >= 1 && inc != 0 ) 
	{	
		Emit( I_INC, inc );
		varscope = scope;
	}
	
	/* Switch statement to ensure correct assignment */
	/* to global variables for later calls */
	switch(var->type)
	{
		/* Assign correct addresses to our INC and DEC calls */
		case STYPE_LOCALVAR:
			dec_local = inc;
			break;
		case STYPE_VARIABLE:
			dec_global = inc; 
			break;
	}
	
	/* Reset inc */
	inc = 0;
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  ParseProcDeclaration implements:                                        */
/*                                                                          */
/* 	 <ProcDeclarati> :==  "PROCEDURE" <Identifier> [<ParameterList>]	    */
/*						  ";" [ <Declarations> ] {<ProcDeclaration>]}	    */											
/*						  <Block> ";"								        */	
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      Emits DEC call for procedures local variables           */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseProcDeclaration( void )
{
	/* Declare backpatch address and procedure symbol variables */
	int backpatch_addr;
	SYMBOL *procedure;
	
	/* Insert our procedure into table */
	/* No need for address for procedures (Only need backpatch addresses */
    Accept( PROCEDURE );
	procedure = MakeSymbolTableEntry( STYPE_PROCEDURE, NULL );
    Accept( IDENTIFIER );
	
	
	backpatch_addr = CurrentCodeAddress();
	/* Emit backpatch call at address of procedure end */
	Emit( I_BR, 0);
	procedure->address = CurrentCodeAddress();
	
	/* Increment scope as we are now inside procedure */
	scope++;
	
	/* Implement [] brackets with if statement */
	if ( CurrentToken.code == LEFTPARENTHESIS) 
	{
		Accept( LEFTPARENTHESIS );
		/* Parse/Compile parameters */
		ParseParameterList( procedure );
		Accept( RIGHTPARENTHESIS );
	}
	Accept( SEMICOLON );
	
	Synchronise(&ProcDecFS1_aug, &ProcDecFBS); 
	if ( CurrentToken.code == VAR ) ParseDeclarations();
	Synchronise(&ProcDecFS2_aug, &ProcDecFBS); 
	
	while ( CurrentToken.code == PROCEDURE ) {
        ParseProcDeclaration();
		Synchronise(&ProcDecFS2_aug, &ProcDecFBS);
    }
	
	/* Parse/Compile procedure block */
	ParseBlock();
	
    Accept( SEMICOLON );
	
	/* Remove variables at end of block */
	/* Emit DEC call for local variables */
	if( dec_local != 0 && scope >= 1 && varscope == scope)
	{
		Emit( I_DEC, dec_local );
		varscope--;
		if( varscope == 0 )
		{
			dec_local = 0;
		}
	}
	
	/* RET call for end of procedure */
	_Emit( I_RET );
	
	/* Used to backpatch to function start when called */
	BackPatch( backpatch_addr, CurrentCodeAddress() );
	
	/* Remove scope when out of procedure and decrement */
	RemoveSymbols( scope );
	scope--;
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  ParseParameterList implements:                                          */
/*                                                                          */
/*	 <ParameterList> :==  "(" <FormalParameter> {"," <FormalParameter}      */
/*						  ")"										        */
/*    Inputs:       Procedure symbol to do work on pcount and ptype 	    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced. Inputs parameters to struct.  */
/* 					Initilizes bitmask for ptype							*/
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseParameterList( SYMBOL *target )
{
	/* Parse/Compile first parameter */
	ParseFormalParameter( target );
	
	/* Initilizes ptype bitmask for procedure */
	target->ptypes = 0x000000b;
	
	/* Check for one or more parameter, if so parse/compile */
	while ( CurrentToken.code == COMMA ) {
		Accept( COMMA );
        ParseParameterList( target );
    }
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  ParseFormalParameter implements:                                        */
/*                                                                          */
/*   <FormalParamet> :==  [ "REF" ] <Variable>						        */
/*    Inputs:       Procedure symbol                                        */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced. Differentiates between REF	*/
/*					and variable parameters for a certain procedure.		*/
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseFormalParameter( SYMBOL *target )
{
	SYMBOL *parameter;
	int token;
	
	/* Increment pcount for each parameter */
	target->pcount++;
	
	/* If statement used to adjust ptype bitmask */
	/* I used the OR operator to add 1 or 0 onto the bitmask */
	if( (token = CurrentToken.code) == REF ) 
	{
		Accept( REF );
		target->ptypes = ( target->ptypes | (2^(target->pcount)) );
		
		/* Assign varaddress to current address for parameter symbol table entry */
		varaddress = CurrentCodeAddress();
		parameter = MakeSymbolTableEntry( STYPE_REFPAR, &varaddress );
		
		/* -1 is for FP offset which is negative */
		/* once again probably better way of doing this */
		parameter->address = -1*varaddress;
	}
	else 
	{
		varaddress = CurrentCodeAddress();
		parameter = MakeSymbolTableEntry( STYPE_VALUEPAR, &varaddress );
		parameter->address = -1*varaddress;
	}
	Accept( IDENTIFIER );
}

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  ParseBlock implements:                                                  */
/*                                                                          */
/*       <Block>  :==  "BEGIN" { <Statement> ";" } "END"                    */
/*                                                                          */
/*                                                                          */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseBlock( void )
{
	/* Emit INC call at beginning of main program block */
	if( scope < 1 && dec_global != 0 ) Emit( I_INC, dec_global);
	
    Accept( BEGIN );
	Synchronise(&StatementFS_aug, &StatementFBS);
	
	/* Check constantly for further Block statements and resynchronise each iteration */
    while ( CurrentToken.code == IDENTIFIER || CurrentToken.code == WHILE
			|| CurrentToken.code == IF || CurrentToken.code == READ ||
				CurrentToken.code == WRITE )   {
        ParseStatement();
        Accept( SEMICOLON );
		Synchronise(&StatementFS_aug, &StatementFBS);
    }
	
    Accept( END );
	
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  ParseStatement implements:                                              */
/*                                                                          */
/*   <Statement>     :==  <SimpleStatement> | <WhileStatement> |            */
/*						  <IfStatement> | <ReadStatement> |                 */
/*						  <WriteStatement>							        */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseStatement( void )
{
	/* SimpleStatement starts with VarOrProcName which is only a variable */
	if( CurrentToken.code == IDENTIFIER ) ParseSimpleStatement(); 
	/* WhileStatement starts with WHILE */
	else if( CurrentToken.code == WHILE ) ParseWhileStatement(); 
	/* IfStatement starts with IF */
	else if( CurrentToken.code == IF ) ParseIfStatement(); 
	/* ReadStatement starts with READ */
	else if( CurrentToken.code == READ ) ParseReadStatement(); 
	/* WriteStatement starts with WRITE */
	else if( CurrentToken.code == WRITE ) ParseWriteStatement();
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  ParseSimpleStatement implements:                                        */
/*                                                                          */
/*   <SimpleStateme> :==  <VarOrProcName> <RestOfStatement>					*/
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced. Advances variable through     */
/* 					to the ParseRestOfStatement( target );					*/
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseSimpleStatement( void )
{
	SYMBOL *target;
	target = LookUpSymbol();	/* Look up IDENTIFIER in lookahead */
	Accept( IDENTIFIER );
	
	/* Pipes variable through to ParseRestOfStatement */
	ParseRestOfStatement( target );
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  ParseRestOfStatement implements:                                        */
/*                                                                          */
/*   <RestOfStateme> :==  <ProcCallList> | <Assignment> | eps 		   		*/
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseRestOfStatement( SYMBOL *target )
{
	int i, dS;
	switch( CurrentToken.code ) {
		case LEFTPARENTHESIS:
			ParseProcCallList( target );
			/* Note fall through for possible arithmetic , FP pushes etc. */
		
		case SEMICOLON:
			if( target != NULL &&
				target->type == STYPE_PROCEDURE )
				{
					/* Check for recursive calls and emit correct frame/stack pushes */
					if( scope == 0 ) _Emit( I_PUSHFP );
					else {
						_Emit( I_LOADFP  );
						for( i = 0; i < scope ; i++ )
						{
							_Emit( I_LOADFP );
						}
					}
					
					/* These three calls will always be present no matter */
					/* the recursion level */
					_Emit( I_BSF );
					Emit( I_CALL, target->address );
					_Emit( I_RSF );
				}
			else {
				Error("Error: Not a procedure!", CurrentToken.pos);
				KillCodeGeneration();
			}
			break;
		case ASSIGNMENT:
		default:
			ParseAssignment();
			/* Check for different types of variables */
			if( target != NULL &&
				target->type == STYPE_VARIABLE )
				Emit( I_STOREA, target->address ); 
			else if( target != NULL && target->type == STYPE_LOCALVAR ) {
				/* For local variables call frame pointer offsets */
				dS = scope - target->scope;
				if( dS == 0 )
					Emit( I_STOREFP, target->address );
				else {
					Emit( I_STOREFP, target->address );
					for( i = 0; i < dS - 1; i++ )
					{
						_Emit( I_STORESP );
					}
					_Emit( I_STORESP );
				}
			}
			else if( target != NULL && (target->type == STYPE_REFPAR || target->type == STYPE_VALUEPAR) )
			{
				/* For parameters it is merely a stack pointer call */
				_Emit( I_STORESP );
			} else {
				Error("Error: Undeclared variable!", CurrentToken.pos);
				KillCodeGeneration();
			}
	}
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  ParseProcCallList implements:                                           */
/*                                                                          */
/*   <ProcCallList>  :==  "(" <ActualParameter> {"," <ActualParameter>}     */
/*						  ")"										        */
/*    Inputs:       SYMBOL *target (procedure)                              */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced. Checks if para is declared.   */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseProcCallList( SYMBOL *target )
{
	int i = 0;
	
	/* Flag to pipe into ParseActualParameter */
	int parflag = 0;
	Accept( LEFTPARENTHESIS );
	
	/* This left shift operation checks for REF parameters (bit '1') */
	if( ((target->ptypes & (1 << i))) != 1 )
	{
		parflag = 1;
		ParseActualParameter(parflag);
	} else ParseActualParameter(parflag);
	
	/* Check for each following parameter and call ParseActualParameter */
	while( CurrentToken.code == COMMA )
	{
		parflag = 0;
		i++;
		Accept( COMMA );
		
		if( (target->ptypes & (1 << (2^i))) != 1 )
		{			
			parflag = 1;
			ParseActualParameter(parflag);
		} else ParseActualParameter(parflag);
	}
	
	Accept( RIGHTPARENTHESIS );
}


/*--------------------------------------------------------------------------*/
/*																			*/
/*  ParseAssignment implements:                                             */
/*                                                                          */
/*   <Assignment>	 :==  ":==" <Expression>						        */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseAssignment( void )
{
	Accept( ASSIGNMENT );
	ParseExpression();
}


/*--------------------------------------------------------------------------*/
/*																			*/
/*  ParseActualParameter implements:                                        */
/*                                                                          */
/*   <ActualParameter> :==  <Variable> | <Expression>					    */
/*    Inputs:       int parflag (flag for type of parameter)	            */
/*					1 for ref par and 0 for value par						*/
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*					Decides how to parse based on type of parameter         */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseActualParameter( int parflag )
{
	SYMBOL *var;
	switch ( parflag )
	{
		case 0:
		default: /* VALUE */
			ParseExpression();
			break;
		case 1: /* REF */
			/* Checks that parameter is actually declared */
			var = LookUpSymbol();
			Accept( IDENTIFIER );			
			Emit( I_LOADI, var->address );
			break;
	}
}


/*--------------------------------------------------------------------------*/
/*																			*/
/*  ParseWhileStatement implements:                                         */
/*                                                                          */
/*	 <WhileStatemen> :==  "WHILE" <BooleanExpression> "DO" <Block>          */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseWhileStatement( void )
{
	int L1, L2, L2BackPatchLoc;
	
	Accept( WHILE );
	L1 = CurrentCodeAddress();
	/* Get backpatch forward address from ParseBooleanExpression() */
	L2BackPatchLoc = ParseBooleanExpression();
	Accept( DO );
	ParseBlock();
	/* Branch to L1 if true */
	Emit( I_BR, L1);
	L2 = CurrentCodeAddress();
	/* Branch to L2 if false */
	BackPatch( L2BackPatchLoc, L2 );
}


/*--------------------------------------------------------------------------*/
/*																			*/
/*  ParseIfStatement implements:                                            */
/*                                                                          */
/*   <IfStatement>   :==  "IF" <BooleanExpression> "THEN" <Block>			*/
/*						  [ "ELSE" <Block> ]                                */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseIfStatement( void )
{
	int L1, L2, L1BackPatchLoc, L2BackPatchLoc;
	Accept( IF );
	/* Get backpatch forward address from ParseBooleanExpression() */
	L1BackPatchLoc = ParseBooleanExpression();
	Accept( THEN );
	ParseBlock();
	/* Branch to L2 if true and skip else */
	L2BackPatchLoc = CurrentCodeAddress();
	Emit( I_BR, L2BackPatchLoc );
	if( CurrentToken.code == ELSE )
	{
		/* Branch to L1 if false and go through else */
		L1 = CurrentCodeAddress();
		BackPatch( L1BackPatchLoc, L1 );
		Accept( ELSE );
		ParseBlock();
	}
	L2 = CurrentCodeAddress();
	BackPatch( L2BackPatchLoc, L2 );
}


/*--------------------------------------------------------------------------*/
/*																			*/
/*  ParseReadStatement implements:                                          */
/*                                                                          */
/*   <ReadStatement> :== "READ" "(" <Variable> {"," <Variable> } ")"        */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseReadStatement( void )
{
	/* Variable(s) to be read */
	SYMBOL *target;
	Accept( READ );
	Accept( LEFTPARENTHESIS );
	
	target = LookUpSymbol();	/* Look up IDENTIFIER in lookahead */
	Accept( IDENTIFIER );
	
	/* Read in each variable in turn */
	_Emit( I_READ );
	if( target != NULL &&
		target->type == STYPE_VARIABLE )
			Emit( I_STOREA, target->address );
	else {
		Error("Error: Undeclared variable!", CurrentToken.pos);
		KillCodeGeneration();
	}
	
	/* Check for more than one variables in Read call */
	while( CurrentToken.code == COMMA )
	{
		Accept( COMMA );
		target = LookUpSymbol();	/* Look up IDENTIFIER in lookahead */
		Accept( IDENTIFIER );
		_Emit( I_READ );
		if( target != NULL &&
			target->type == STYPE_VARIABLE )
			Emit( I_STOREA, target->address);
		else {
			Error("Error: Undeclared variable!", CurrentToken.pos);
			KillCodeGeneration();
		}
	}
	
	/* End of Read statement */
	Accept( RIGHTPARENTHESIS );
}


/*--------------------------------------------------------------------------*/
/*																			*/
/*  ParseWriteStatement implements:                                         */
/*                                                                          */
/*   <WriteStatemen> :== "WRITE" "(" <Expression> { "," <Expression>        */
/*						 } ")"										        */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseWriteStatement( void )
{
	/* Parse through Write call */
	Accept( WRITE );
	Accept( LEFTPARENTHESIS );
	
	/* Let ParseExpression() deal with variables & arithmetic in Write call */
	ParseExpression();
	_Emit( I_WRITE );
	
	/* Check for more than one variables in Write call */
	while( CurrentToken.code == COMMA ){
		Accept( COMMA );
		ParseExpression();
		_Emit( I_WRITE );
	}
	
	/* End of Write statement */
	Accept( RIGHTPARENTHESIS );
}


/*--------------------------------------------------------------------------*/
/*																			*/
/*  ParseExpression implements:                                             */
/*                                                                          */
/*	 <Expression>    :== <CompoundTerm> { <AddOp> <CompoundTerm> }	   	    */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseExpression( void )
{
	int token;
	ParseCompoundTerm();
	
	/* Check for simple arithmetic +/- */
	while( (token = CurrentToken.code) == ADD ||
			token == SUBTRACT ) {
		Accept( token );
		/* ParseCompoundTerm() goes through * or / */
		ParseCompoundTerm();
		
		/* Emit correct machine calls depending on operators */
		if( token == ADD ) _Emit( I_ADD ); else _Emit( I_SUB );
	}
}


/*--------------------------------------------------------------------------*/
/*																			*/
/*  ParseCompoundTerm implements:                                           */
/*                                                                          */
/* 	 <CompoundTerm>  :== <Term> { <MultOP> <Term> }					        */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseCompoundTerm( void )
{
	int token;
	ParseTerm();
	
	/* Check for simple arithmetic * or / */
	while( (token = CurrentToken.code) == MULTIPLY ||
			token == DIVIDE ) {
		Accept( token );
		ParseTerm();
		
		/* Emit correct machine calls depending on operators */
		if( token == MULTIPLY ) _Emit( I_MULT ); else _Emit( I_DIV );
	}
}


/*--------------------------------------------------------------------------*/
/*																			*/
/*  ParseTerm implements:                                                   */
/*                                                                          */
/*   <Term>          :== ["-"] <SubTerm>							        */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseTerm( void )
{
	/* negateflag is used for to check for negative numbers */
	int negateflag = 0;
	
	if( CurrentToken.code ==  SUBTRACT ) {
		Accept( SUBTRACT );
		negateflag = 1;
	}
	ParseSubTerm();
	
	/* if negative number do */
	if( negateflag ) _Emit( I_NEG );
}


/*--------------------------------------------------------------------------*/
/*																			*/
/*  ParseSubTerm implements:                                                */
/*                                                                          */
/*	 <SubTerm>		 :== <Variable> | <IntConst> | "(" <Expression> ")"     */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseSubTerm( void )
{
	int i, dS;
	SYMBOL *var;
	
	if( CurrentToken.code == IDENTIFIER ) 
	{
		/* Check variable in Symbol table */
		var = LookUpSymbol();
		Accept( IDENTIFIER );
		if( var != NULL ) 
		{
			/* Simple LOAD call for global variables */
			if( var->type == STYPE_VARIABLE )
			{
				Emit( I_LOADA, var->address );
			} else if( var->type == STYPE_LOCALVAR )
			{
				/* Load frame pointer offsets for local variables based on scope */
				dS = scope - var->scope;
				if( dS == 0 )
				{
					Emit( I_LOADFP, (var->address) );
				} else {
					/* Stack pointer is used for procedures with scopes > 1 */
					_Emit( I_LOADFP );
					for( i = 0; i < dS - 1; i++ )
					{
						_Emit( I_LOADSP );
					}
					Emit( I_LOADSP, var->address );
				}
			/* Frame pointer is also used for parameters */
			} else if( var->type == STYPE_REFPAR || var->type == STYPE_VALUEPAR )
			{
				for( i = var->address; i < 0; i++ )
				{
					Emit( I_LOADFP, i );
				}
			}
		} else { /* Error checking */
			Error("Name undeclared or is not a variable!", CurrentToken.pos);
			KillCodeGeneration();
		}		
	}
	/* Simple LOAD call for integers */
	else if( CurrentToken.code == INTCONST ) {
		Emit( I_LOADI, CurrentToken.value );
		Accept( INTCONST );
	}
	/* ParseExpression() for arithmetic */
	else {
		Accept( LEFTPARENTHESIS );
		ParseExpression();
		Accept( RIGHTPARENTHESIS );
	}
}


/*--------------------------------------------------------------------------*/
/*																			*/
/*  ParseBooleanExpression implements:                                      */
/*                                                                          */
/* 	 <BooleanExpre>  :== <Expression> <RelOp> <Expression>                  */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Backpatch address BackPatchAddr                         */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE int ParseBooleanExpression( void )
{
	int BackPatchAddr, RelOpInstruction;
	ParseExpression();
	/* ParseRelOp returns Boolean instruction for while/if */
	RelOpInstruction = ParseRelOp();
	ParseExpression();
	_Emit( I_SUB );
	
	/* Returns backpatch address of boolean expression */
	BackPatchAddr =  CurrentCodeAddress();
	
	/* Dummy target address which will be backpatched later by caller */
	Emit( RelOpInstruction, 999 );
	return BackPatchAddr;
}


/*--------------------------------------------------------------------------*/
/*																			*/
/*  ParseRelOp implements:                                                  */
/*                                                                          */
/*   <RelOp>		 :== "=" | "<=" | ">=" | "<" | ">"				   	    */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Boolean inverse instruction		                        */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE int ParseRelOp( void )
{
	int RelOpInstruction;
	
	switch( CurrentToken.code ) 
	{
		/* Boolean inverse instructions are used */
		case LESSEQUAL:
			RelOpInstruction = I_BG; Accept( LESSEQUAL );
			break;
		case GREATEREQUAL:
			RelOpInstruction = I_BL; Accept( GREATEREQUAL );
			break;
		case LESS:
			RelOpInstruction = I_BGZ; Accept( LESS );
			break;
		case GREATER:
			RelOpInstruction = I_BLZ; Accept( GREATER );
			break;
		case EQUALITY:
			RelOpInstruction = I_BNZ; Accept( EQUALITY );
			break;
	}
	/* Return negated instruction */
	return RelOpInstruction;
}

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  End of parser.  Support routines follow.                                */
/*                                                                          */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  Accept:  Takes an expected token name as argument, and if the current   */
/*           lookahead matches this, advances the lookahead and returns.    */
/*                                                                          */
/*                                                                          */
/*    Inputs:       Integer code of expected token                          */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: If successful, advances the current lookahead token     */
/*                  "CurrentToken". Recovers if error is encountered        */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void Accept( int ExpectedToken )
{
	static int recovering = 0;
	
	/* Error resync code */
	if (recovering) {
		while ( CurrentToken.code != ExpectedToken &&
				CurrentToken.code != ENDOFINPUT ) {
					CurrentToken = GetToken();
				}
				/* Reset flag */
				recovering = 0;
	}
	
	/* Normal Accept code */
	if ( CurrentToken.code != ExpectedToken ) {
		/* Display error message */
		SyntaxError(ExpectedToken, CurrentToken);
		/* Set flag when error is encountered */
		recovering = 1;
	}
	else CurrentToken = GetToken();
	
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  OpenFiles:  Reads strings from the command-line and opens the           */
/*              associated input and listing files.                         */
/*                                                                          */
/*    Note that this routine modifies the globals "InputFile", "CodeFile"   */
/*    "ListingFile".  It returns 1 ("true" in C-speak) if the input and     */
/*    listing files are successfully opened, 0 if not, allowing the caller  */
/*    to make a graceful exit if the opening process failed.                */
/*                                                                          */
/*                                                                          */
/*    Inputs:       1) Integer argument count (standard C "argc").          */
/*                  2) Array of pointers to C-strings containing arguments  */
/*                  (standard C "argv").                                    */
/*                                                                          */
/*    Outputs:      No direct outputs, but note side effects.               */
/*                                                                          */
/*    Returns:      Boolean success flag (i.e., an "int":  1 or 0)          */
/*                                                                          */
/*    Side Effects: If successful, modifies globals "InputFile",            */
/*                  "ListingFile" and "CodeFile".                           */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE int  OpenFiles( int argc, char *argv[] )
{
	/* Four files needed when running program */
    if ( argc != 4 )  {
        fprintf( stderr, "%s <inputfile> <listfile> <codefile>\n", argv[0] );
        return 0;
    }

    if ( NULL == ( InputFile = fopen( argv[1], "r" ) ) )  {
        fprintf( stderr, "cannot open \"%s\" for input\n", argv[1] );
        return 0;
    }

    if ( NULL == ( ListFile = fopen( argv[2], "w" ) ) )  {
        fprintf( stderr, "cannot open \"%s\" for output\n", argv[2] );
        fclose( InputFile );
        return 0;
    }
	
	if ( NULL == ( CodeFile = fopen( argv[3], "w" ) ) )  {
        fprintf( stderr, "cannot open \"%s\" for output\n", argv[3] );
        fclose( CodeFile );
        return 0;
    }

    return 1;
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  Synchronise:                                                            */
/*      Resynchronises the parser after encountering an error in the        */
/*		input file.															*/
/*                                                                          */
/*                                                                          */
/*    Inputs:       1) Augmented pointer set.                               */
/*                  2) Follow U Beacons set.                                */
/*                                                                          */
/*    Outputs:      No direct outputs, but note side effects.               */
/*                                                                          */
/*    Returns:      Nothing         									    */
/*                                                                          */
/*    Side Effects: Helps the parser to get back on track when parsing		*/
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void Synchronise( SET *F, SET *FB )
{
	SET S;
	
	/* Resynchronise code using predict/follow set arguments */
	S = Union(2, F, FB);
	if( !InSet(F, CurrentToken.code ) ) {
		SyntaxError2( *F, CurrentToken );
		while( !InSet( &S, CurrentToken.code ) )
			CurrentToken = GetToken();
	}
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  SetupSets:                                                              */
/*            Initilizes primary synchronisation sets for a number of       */
/*			  functions using InitSet() from sets.h                         */
/*                                                                          */
/*                                                                          */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      No direct outputs, but note side effects.               */
/*                                                                          */
/*    Returns:      Nothing         									    */
/*                                                                          */
/*    Side Effects: Initilizes all relevent sets for primary sync points	*/
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void SetupSets( void )
{
	
	/* ParseProgram */
	InitSet( &ProgramFS1_aug, 3, VAR, PROCEDURE,
			BEGIN );
	InitSet( &ProgramFS2_aug, 2, PROCEDURE, BEGIN );
	InitSet( &ProgramFBS, 3, ENDOFINPUT, ENDOFPROGRAM,
			 END );
	
	/* ParseProcDeclaration */
	InitSet( &ProcDecFS1_aug, 3, VAR, PROCEDURE,
			 BEGIN );
	InitSet( &ProcDecFS2_aug, 2, PROCEDURE, BEGIN );
	InitSet( &ProcDecFBS, 3, ENDOFPROGRAM, ENDOFINPUT,
			 END );
	
	/* ParseBlock */
	InitSet( &StatementFS_aug, 6, IDENTIFIER, WHILE,
			 IF, READ, WRITE, END );
	InitSet( &StatementFBS, 4, SEMICOLON, ELSE,
			 ENDOFPROGRAM, ENDOFINPUT );
	
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  MakeSymbolTableEntry:                                                   */
/*            Enters a variable/procedure into the symbol table and 		*/
/*			  allocates memory to it.										*/
/*                                                                          */
/*                                                                          */
/*    Inputs:       symtype; type of symbol to be inputted as integer       */
/*					varaddress; pointer to symbol address 					*/
/*                                                                          */
/*    Outputs:      No direct outputs, but note side effects.               */
/*                                                                          */
/*    Returns:      new SYMBOL pointer        							    */
/*                                                                          */
/*    Side Effects: Makes a new entry to symbol table						*/
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE SYMBOL *MakeSymbolTableEntry( int symtype, int *varaddress )
{
	SYMBOL *oldsptr, *newsptr;
	char *cptr;
	int hashindex;
	
	if( CurrentToken.code == IDENTIFIER ) {
		/* Checks to see if there is already an entry for this variable */
		if( NULL == ( oldsptr = Probe(CurrentToken.s, &hashindex)) || oldsptr->scope < scope ) 
		{
			/* Reuse strings if there is already a SYMBOL of the same name present */
			if( oldsptr == NULL ) cptr = CurrentToken.s; else cptr = oldsptr->s;
			/* Make a new entry in symbol table */
			if( NULL == ( newsptr = EnterSymbol(cptr, hashindex))) {
				Error("Fatal internal error in EnterSymbol", CurrentToken.pos);
				KillCodeGeneration();
			}
			else 
			{
				/* Fill in SYMBOL fields */
				if( oldsptr == NULL ) PreserveString();
				newsptr->scope = scope;
				newsptr->type = symtype;
				/* Address allocation */
				if( symtype == STYPE_VARIABLE || symtype == STYPE_LOCALVAR ) {
					newsptr->address = *varaddress; (*varaddress)++;
				}
				/* else dummy address */
				else newsptr->address = -1;
			}
		}
		else /* Error call and kill code gen */
		{
			Error("Error! Variable already declared", CurrentToken.pos);
			KillCodeGeneration();			
		}
	}
	/* Returns new symbol pointer */
	return newsptr;
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  LookUpSymbol:                                              		        */
/*            Checks if a variable has already been declared				*/
/*                                                                          */
/*                                                                          */
/*    Inputs:       None											        */
/*                                                                          */
/*    Outputs:      No direct outputs, but note side effects.               */
/*                                                                          */
/*    Returns:      Symbol pointer sptr with string name of variable        */
/*                                                                          */
/*    Side Effects: Kills code generation if variable not declared			*/
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE SYMBOL *LookUpSymbol( void )
{
	SYMBOL *sptr;
	
	/* Probes symbol table to see if variable is declared */
	if( CurrentToken.code == IDENTIFIER ) {
		sptr = Probe( CurrentToken.s, NULL );
		if( sptr == NULL ) {
			Error("Identifier not declared", CurrentToken.pos);
			KillCodeGeneration();
		}
	}
	else sptr = NULL;
	return sptr;
}
				