/*--------------------------------------------------------------------------*/
/*                                                                          */
/*       parser2.c      											        */
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
#include "sets.h"
#include "line.h"


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  Global variables used by this parser.                                   */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE FILE *InputFile;           /*  CPL source comes from here.          */
PRIVATE FILE *ListFile;            /*  For nicely-formatted syntax errors.  */

PRIVATE TOKEN  CurrentToken;       /*  Parser lookahead token.  Updated by  */
                                   /*  routine Accept (below).  Must be     */
                                   /*  initialised before parser starts.    */

PRIVATE	SET ProgramFS1_aug, ProgramFS2_aug, ProgramFBS, /* Synchronise Sets */
			ProcDecFS1_aug, ProcDecFS2_aug, ProcDecFBS, 
			StatementFS_aug, StatementFBS;


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
PRIVATE void ParseRestOfStatement( void );
PRIVATE void ParseProcCallList( void );
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
PRIVATE void ParseBooleanExpression( void );

PRIVATE int  OpenFiles( int argc, char *argv[] );
PRIVATE void Accept( int code );
PRIVATE void Synchronise( SET *F, SET *FB );
PRIVATE void SetupSets( void );


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  Main: Program entry point.  Sets up parser globals (opens input and     */
/*        output files, initialises current lookahead), then calls          */
/*        "ParseProgram" to start the parse.                                */
/* 		   Calls SetupSets() to assign synchronisation sets					*/
/*                                                                          */
/*--------------------------------------------------------------------------*/

PUBLIC int main ( int argc, char *argv[] )
{
	SetupSets();
    if ( OpenFiles( argc, argv ) )  {
        InitCharProcessor( InputFile, ListFile );
        CurrentToken = GetToken();
        ParseProgram();
        fclose( InputFile );
        fclose( ListFile );
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
	
	/* Parse variables if present */
    if ( CurrentToken.code == VAR )  ParseDeclarations();
	
	Synchronise(&ProgramFS2_aug, &ProgramFBS);
	
	/* Checks for one or more procedures */
	while ( CurrentToken.code == PROCEDURE ) {
        ParseProcDeclaration();
		Synchronise(&ProgramFS2_aug, &ProgramFBS);
    }
	
	/* Begin parsing main block of code */
    ParseBlock();
    Accept( ENDOFPROGRAM );     /* Token "." has name ENDOFPROGRAM          */
	Accept( ENDOFINPUT );
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
    Accept( VAR );
    Accept( IDENTIFIER );   /* <Variable> is just a remaning of IDENTIFIER. */
    
    /* EBNF repetition operator {...} implemented as a while-loop.          */
    /* Repetition triggered by "," (i.e, COMMA) in lookahead.               */
	
    while ( CurrentToken.code == COMMA ) {
        Accept( COMMA );
        Accept( IDENTIFIER );
    }
    Accept( SEMICOLON );
	
}


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  ParseProcDeclaration implements:                                       */
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
    Accept( IDENTIFIER );
	
	/* Implement [] brackets with if statement */
	if ( CurrentToken.code == LEFTPARENTHESIS) 
	{
		Accept( LEFTPARENTHESIS );
		/* Parse/Compile parameters */
		ParseParameterList();
		Accept( RIGHTPARENTHESIS );
	}
	Accept( SEMICOLON );
	
	/* Resynchronise with augmented sets if error occurs */
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
	Accept( IDENTIFIER );
	ParseRestOfStatement();
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

PRIVATE void ParseRestOfStatement( void )
{
	if ( CurrentToken.code == LEFTPARENTHESIS ) ParseProcCallList();
	else if ( CurrentToken.code == ASSIGNMENT ) ParseAssignment();
	/* Accepts null (eps) */
	else ;
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

PRIVATE void ParseProcCallList( void )
{
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
	Accept( WHILE );
	
	/* Parse conditions */
	ParseBooleanExpression();
	
	/* Parse DO block */
	Accept( DO );
	ParseBlock();
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
	Accept( IF );
	
	/* Parse conditions */
	ParseBooleanExpression();
	
	/* Parse THEN block */
	Accept( THEN );
	ParseBlock();
	
	if( CurrentToken.code == ELSE ){
		/* Parse ELSE block */
		Accept( ELSE );
		ParseBlock();
	}
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
	/* Parse READ statement */
	Accept( READ );
	Accept( LEFTPARENTHESIS );
	
	/* Read in each variable in turn */
	Accept( IDENTIFIER );
	
	/* Check for more than one variables in Read call */
	while( CurrentToken.code == COMMA ){
		Accept( COMMA );
		Accept( IDENTIFIER );
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
	
	/* Check for more than one variables in Write call */
	while( CurrentToken.code == COMMA ){
		Accept( COMMA );
		ParseExpression();
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
	if( CurrentToken.code ==  SUBTRACT ) Accept( SUBTRACT );
	ParseSubTerm();
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
	/* ParseSubTerm handles variables, integer constants and some arithmetic */
	if( CurrentToken.code == IDENTIFIER ) Accept( IDENTIFIER );
	else if( CurrentToken.code == INTCONST ) Accept( INTCONST );
	else {
		Accept( LEFTPARENTHESIS );
		/* ParseExpression() for arithmetic */
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

PRIVATE void ParseBooleanExpression( void )
{
	ParseExpression();
	
	/* Check for each boolean expression in turn */
	if( CurrentToken.code == EQUALITY ) Accept( EQUALITY );
	else if( CurrentToken.code == LESSEQUAL ) Accept( LESSEQUAL );
	else if( CurrentToken.code == GREATEREQUAL ) Accept( GREATEREQUAL );
	else if( CurrentToken.code == LESS ) Accept( LESS );
	else Accept( GREATER );
	
	ParseExpression();
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
/*    Note that this routine modifies the globals "InputFile" and           */
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
/*    Side Effects: If successful, modifies globals "InputFile" and         */
/*                  "ListingFile".                                          */
/*                                                                          */
/*--------------------------------------------------------------------------*/

PRIVATE int  OpenFiles( int argc, char *argv[] )
{
	/* Three files needed when running program */
    if ( argc != 3 )  {
        fprintf( stderr, "%s <inputfile> <listfile>\n", argv[0] );
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