/*
 * Various outputs for flag of json_object_to_json_string_ext().
 *
 * gcc -Wall -I/usr/include/json-c/ -o json_print json_print.c -ljson-c
 */
#include <json.h>
#include <stdio.h>
 
int main() {
	struct json_object *jobj, *jobj_apo;
	const char *question = "Mum, clouds hide alien spaceships don't they ?";
	const char *answer = "Of course not! (\"sigh\")";
	const char *dog = "Arlo is his name-o";
	int i;
	struct {
		int flag;
		const char *flag_str;
	} json_flags[] = {
		{ JSON_C_TO_STRING_PLAIN, "JSON_C_TO_STRING_PLAIN" },
		{ JSON_C_TO_STRING_SPACED, "JSON_C_TO_STRING_SPACED" },
		{ JSON_C_TO_STRING_PRETTY, "JSON_C_TO_STRING_PRETTY" },
		{ JSON_C_TO_STRING_NOZERO, "JSON_C_TO_STRING_NOZERO" },
		{ JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY, "JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY" },
		{ -1, NULL }
	}; // Create an anonymous struct, instanciate an array and fill it
 
	printf("Using printf(): \"%s\", \"%s\"\n\n", question, answer);
	printf("Using json_object_to_json_string_ext():\n");
 
	/*
	 * The following create an object and add the question and answer to it.
	 */
	jobj = json_object_new_object();
	jobj_apo = json_object_new_object();

//	json_object_object_add(jobj, "dog", json_object_new_string(dog));
	json_object_object_add(jobj_apo, "dog", json_object_new_int(8));
	json_object_object_add(jobj, "apo", jobj_apo);

	json_object_object_add(jobj, "question", json_object_new_string(question));
	json_object_object_add(jobj, "answer", json_object_new_string(answer));
 
	for (i = 0; json_flags[i].flag_str != NULL; i++) {
		printf("\nFlag %s:\n---\n", json_flags[i].flag_str);
		printf("%s\n---\n", json_object_to_json_string_ext(jobj, json_flags[i].flag));
	}
 
	json_object_put(jobj); // Delete the json object
 
	return 0;
}
