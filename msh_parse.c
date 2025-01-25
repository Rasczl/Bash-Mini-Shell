#include <msh_parse.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
//My struct for a command
//comEntries is a string array of args
//comString is the command line it wants to store
//numArgs is the number of Arguments in the command
//final is a boolean which checks if it's the final command in a pipeline
struct msh_command {
	char* comEntires[MSH_MAXARGS];
	char* comString;
	int numArgs;
	//bool
	int final;
	void *data;
	msh_free_data_fn_t freefn;
};
//My struct for Pipeline
//pipEntries is a command array 
//pipString is the pipeline it wants to store
//numCommand is the number of commands in the pipeline
struct msh_pipeline {
	struct msh_command* pipEntries[MSH_MAXCMNDS];
	char* pipString;
	int numCommand;
};
//My struct for Sequence
//seqEntries is an array of pipelines
//numPipeline is the number of pipelines in the sequence
struct msh_sequence {
	struct msh_pipeline* seqEntries[MSH_MAXBACKGROUND];
	int numPipeline;
};
// A helper method used to free all the arguments array and the command
void
msh_command_free(struct msh_command *c)
{
	//base case
	if(c == NULL){
		return;
	}
	//itterate to free every arg
	for(int i = 0; i < c->numArgs; i++){
		free(c->comEntires[i]);
	}
	//free the command and it's string
	free(c->comString);
	free(c);

}

//A helper method to free all the commands in a pipeline and itself
void
msh_pipeline_free(struct msh_pipeline *p)
{
	//NULL checker
	if(p == NULL){
		return;
	}
	//Free all the command nodes
	for(int i = 0; i < p->numCommand; i++){
		//Free each command using reccursion
		msh_command_free(p->pipEntries[i]);
	}
	//Free all the pipeline nodes and it's string
	free(p->pipString);
	free(p);
}

//The biggest free method which free's the entire program
void
msh_sequence_free(struct msh_sequence *s)
{
	//NULL checker
	if(s == NULL){
		return;
	}
	//Frees all the Pipeline using reccursion from the free pipeline method above
	for(int i = 0; i < s->numPipeline; i++){
		//Free each pipeline using reccursion
		msh_pipeline_free(s->seqEntries[i]);
	}
	//Free the sequence, this freeing the entire program
	free(s);
}

struct msh_sequence*
msh_sequence_alloc(void)
{
	//Allocate memory for the Sequence and initialize all variabless
	struct msh_sequence *temp = malloc(sizeof(struct msh_sequence));
	temp->numPipeline = 0;
	//Base case
	if(temp == NULL){
		return NULL;
	}
	//Intialize all entries
	for(int i = 0; i < MSH_MAXBACKGROUND; i++){
		temp->seqEntries[i] = NULL;
	}
	//return sequence
	return temp;

}

//returns the String used as an Input for the pipeline
char *
msh_pipeline_input(struct msh_pipeline *p)
{
	//Base case
	if(p == NULL){
		return NULL;
	}
	//Returns the String stored in the pipeline
	return p->pipString;
}

//A parsing method to Parse the str into the sequence struct
msh_err_t
msh_sequence_parse(char *str, struct msh_sequence *seq)
{
	//Base case
	if(str == NULL || seq == NULL){
		// return msg_err_t;
		return -1;
	}
	//Creates a deep copy of str so we can modify it w/o modifying the param
	char* temp = strdup(str);
	//Malloc check
	if(temp == NULL){
		return -1;
	}
	//Initialize the variables
	//The token1-3 represents from a pipeline to a command to an argument
	char* token1;
	char* token2;
	char* token3;
	//Used for strtok_r
	char* pointer1;
	char* pointer2;
	char* pointer3;
	//Parse the temp str from a sequence which turns into multiple pipelines
	for(token1 = strtok_r(temp, ";", &pointer1); token1 != NULL; token1 = strtok_r(pointer1, ";", &pointer1)){
		//Allocate memory for a pipeline it's about to insert
		struct msh_pipeline *pip = malloc(sizeof(struct msh_pipeline));
		//Intialize
		pip->numCommand = 0;
		//Store the String
		pip->pipString = strdup(token1);
		//Initialize
		for(int i = 0; i < MSH_MAXCMNDS; i++){
			pip->pipEntries[i] = NULL;
		}
		//Parse each of the pipeline into multiple commands
		for(token2 = strtok_r(token1, "|", &pointer2); token2 != NULL; token2 = strtok_r(pointer2, "|", &pointer2)){
			//Allocate memory for the command it's about to insert
			struct msh_command *com = malloc(sizeof(struct msh_command));
			//Initialize
			for(int j = 0; j < MSH_MAXARGS; j++){
				com->comEntires[j] = NULL;
			}
			com->numArgs = 0;
			com->final = 0;
			//Stores the string
			com->comString = strdup(token2);
			//Parse each of the command into multiple Args
			for(token3 = strtok_r(token2, " ", &pointer3); token3 != NULL; token3 = strtok_r(pointer3, " ", &pointer3)){
				//Insert each arguments into the *char[] which stores args
				com->comEntires[com->numArgs] = strdup(token3);
				//Increment num of args
				com->numArgs++;
			}
			//Breaks out of the first for loop and after we succesfullpy parsed the command
			//Insert the command into the last entry in pipeline
			pip->pipEntries[pip->numCommand] = com;
			//Increment
			pip->numCommand++;			
		}
		//Sets the last comman in the pipeline to final
		pip->pipEntries[pip->numCommand - 1]->final = 1;
		//Insert toe pipeline into the last entry of a sequence
		seq->seqEntries[seq->numPipeline] = pip;
		//Increment
		seq->numPipeline++;
	}
	//Free the temp variable we malloc
	free(temp);
	//return successfully
	return 0;
}


//Dequeues first pipeline in a Sequence
struct msh_pipeline *
msh_sequence_pipeline(struct msh_sequence *s)
{
	//Base case
	if(s == NULL || s->numPipeline == 0){
		return NULL;
	}
	//Store the first entry too return
	struct msh_pipeline* pip = s->seqEntries[0];
	//Base case
	if(pip == NULL){
		return NULL;
	}
	//Shift every single pipeline up
	for(int i = 1 ; i < s->numPipeline; i++){
		s->seqEntries[i - 1] = s->seqEntries[i];
	}
	//Decrement
	s->numPipeline--;
	//returns the first entry it removed
	return pip;
}

//Quereris a specific command in a pipeline
struct msh_command *
msh_pipeline_command(struct msh_pipeline *p, size_t nth)
{
	//base case
	if(p == NULL){
		return NULL;
	}
	//returns a certain index at a certain pipeline array to find a command
	return p->pipEntries[nth];
}

//Checks if a pipeline should be run in the background or not
int
msh_pipeline_background(struct msh_pipeline *p)
{
	//Stores the last command in a pipeline
	struct msh_command *lastcommand = p->pipEntries[p->numCommand - 1];
	//Checks if the last command was &
	if((strcmp(lastcommand->comEntires[lastcommand->numArgs - 1], "&") == 0)){
		//If so return true
		return 1;
	}
	//else flase
	return 0;
}
//Returns T/F if a certain command is the final in a pipeline
int
msh_command_final(struct msh_command *c)
{
	//base case
	if(c == NULL){
		return 0;
	}
	//Returns 1(True), or 0(False)
	return c->final;
}
//Skip for M0
void
msh_command_file_outputs(struct msh_command *c, char **stdout, char **stderr)
{
	(void)c;
	(void)stdout;
	(void)stderr;
}

// Retrieves the program to be executed for a command
char *
msh_command_program(struct msh_command *c)
{
	//base case
	if (c == NULL) {
        return NULL;
    }
	//Returns the first argument in a command
    return c->comEntires[0]; 
}
//Retrieve the entire Arguments in a command
char **
msh_command_args(struct msh_command *c)
{
	//Base case
    if (c == NULL) {
        return NULL;
    }
	//Returns the entire array of Args
    return c->comEntires;
}
//Skip M0
void
msh_command_putdata(struct msh_command *c, void *data, msh_free_data_fn_t freefn)
{
	if(c == NULL){
		return;
	}
	if(c->data != NULL || c->freefn != NULL){
		c->data = data;
		c->freefn = freefn;
	}
}
//Skip M0
void *
msh_command_getdata(struct msh_command *c)
{
	if(c == NULL){
		return NULL;
	}
	return c->data;

}
