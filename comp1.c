/*--------------------------------------------------------------------------*/
/*                                                                          */
/*       comp1.c      											            */
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

/* Scope for procedure scope and varaddress for symbol addresses */
PRIVATE int scope, varaddress;


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  Function prototypes                                                     */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseProgram( void );
PRIVATE void ParseDeclarations( void );
PRIVATE void ParseProcDeclaration( void );
PRIVATE void ParseParameterList ( void );
PRIVATE void ParseFormalParameter( void );
PRIVATE void ParseBlock( void );
PRIVATE void ParseStatement( void );
PRIVATE void ParseSimpleStatement( void );
PRIVATE void ParseRestOfStatement( SYMBOL *target );
PRIVATE void ParseProcCallList( SYMBOL *target );
PRIVATE void ParseAssignment( void );
PRIVATE void ParseActualParameter( void );
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
PRIVATE void MakeSymbolTableEntry( int symtype );
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
/*    Outputs:      None                                                    */
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
    Accept( ENDOFPROGRAM );     /* Token "." has name ENDOFPROGRAM          */
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
	int inc = 1;			/* counter of no. of variable declarations */
    Accept( VAR );
	MakeSymbolTableEntry( STYPE_VARIABLE );
    Accept( IDENTIFIER );   /* <Variable> is just a remaning of IDENTIFIER. */
    
    /* EBNF repetition operator {...} implemented as a while-loop.          */
    /* Repetition triggered by "," (i.e, COMMA) in lookahead.               */

    while ( CurrentToken.code == COMMA ) {
        Accept( COMMA );
		MakeSymbolTableEntry( STYPE_VARIABLE );
        Accept( IDENTIFIER );
		/* Increment  counter for each global */
		inc++;
    }
    Accept( SEMICOLON );
	
	/* Increment Stack Pointer */
	Emit( I_INC, inc );
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
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseProcDeclaration( void )
{
    Accept( PROCEDURE );
	
	/* Insert our procedure into symbol table */
	MakeSymbolTableEntry( STYPE_PROCEDURE );
    Accept( IDENTIFIER );
	/* Increment scope when in procedure */
	scope++;
	
	/* Implement [] brackets with if statement */
	if ( CurrentToken.code == LEFTPARENTHESIS) 
	{
		Accept( LEFTPARENTHESIS );
		/* Parse/Compile parameters */
		ParseParameterList();
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
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseParameterList( void )
{	
	/* Parse/Compile first parameter */
	ParseFormalParameter();
	
	/* Check for one or more parameter, if so parse/compile */
	while ( CurrentToken.code == COMMA ) {
		Accept( COMMA );
        ParseParameterList();
    }
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  ParseFormalParameter implements:                                        */
/*                                                                          */
/*   <FormalParamet> :==  [ "REF" ] <Variable>						        */
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseFormalParameter( void )
{
	int token;
	
	/* Check for reference parameter */
	if( (token = CurrentToken.code) == REF ) Accept( REF );
	
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
	
	/* End of Block */
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
/*    Side Effects: Lookahead token advanced.                               */
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
	switch( CurrentToken.code ) {
		case LEFTPARENTHESIS:
			ParseProcCallList( target );
			/* Note fall through for possible arithmetic , FP pushes etc. */
		
		case SEMICOLON:
			if( target != NULL &&
				target->type == STYPE_PROCEDURE )
				/* Dummy address for proc calls */
				Emit( I_CALL, 999 );
			else {
				Error("Error: Not a procedure!", CurrentToken.pos);
				KillCodeGeneration();
			}
			break;
		case ASSIGNMENT:
		default:
			ParseAssignment();
			/* Make Store call for global variables */
			if( target != NULL &&
				target->type == STYPE_VARIABLE )
				Emit( I_STOREA, target->address );
			else { /* Error check */
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
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseProcCallList( SYMBOL *target )
{
	target = LookUpSymbol();
	Accept( LEFTPARENTHESIS );
	
	/* Parse first parameter */
	ParseActualParameter();
	
	/* Check for more parameters */
	while( CurrentToken.code == COMMA ){
		Accept( COMMA );
		ParseActualParameter();
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
/*    Inputs:       None                                                    */
/*                                                                          */
/*    Outputs:      None                                                    */
/*                                                                          */
/*    Returns:      Nothing                                                 */
/*                                                                          */
/*    Side Effects: Lookahead token advanced.                               */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void ParseActualParameter( void )
{
	if( CurrentToken.code == IDENTIFIER ) Accept( IDENTIFIER );
	else ParseExpression();
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
	SYMBOL *var;
	
	if( CurrentToken.code == IDENTIFIER ) 
	{
		/* Check variable in Symbol table */
		var = LookUpSymbol();
		Accept( IDENTIFIER );
		/* Simple LOAD call for global variables */
		if( var != NULL ) Emit( I_LOADA, var->address );
		else {
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
/*    Returns:      Nothing                                                 */
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
/*                                                                          */
/*    Outputs:      No direct outputs, but note side effects.               */
/*                                                                          */
/*    Returns:      Nothing         									    */
/*                                                                          */
/*    Side Effects: Makes a new entry to symbol table						*/
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE void MakeSymbolTableEntry( int symtype )
{
	SYMBOL *oldsptr, *newsptr;
	char *cptr;
	int hashindex;
	
	if( CurrentToken.code == IDENTIFIER ) {
		/* Checks to see if there is already an entry for this variable */
		if( NULL == ( oldsptr = Probe(CurrentToken.s, &hashindex)) || oldsptr->scope < scope ) {
			/* Reuse strings if there is already a SYMBOL of the same name present */
			if( oldsptr == NULL ) cptr = CurrentToken.s; else cptr = oldsptr->s;
			/* Make a new entry in symbol table */
			if( NULL == ( newsptr = EnterSymbol(cptr, hashindex))) {
				Error("Fatal internal error in EnterSymbol", CurrentToken.pos);
				KillCodeGeneration();
			}
			else {
				/* Fill in SYMBOL fields */
				if( oldsptr == NULL ) PreserveString();
				newsptr->scope = scope;
				newsptr->type = symtype;
				/* Address allocation */
				if( symtype == STYPE_VARIABLE ) {
					newsptr->address = varaddress; varaddress++;
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
/*    Side Effects: Kills code generation of variable not declared			*/
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
				