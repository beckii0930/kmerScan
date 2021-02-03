#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include "htslib/htslib/sam.h"
#include "htslib/htslib/khash.h"
// #include "htslib-1.11/htslib/hts.h"  

#define CAPACITY 50000 // Size of the Hash Table
int kmer_length = 0; 

unsigned long hash_function(const char* key) { 
    int i=0; int j; int k;
    j = k = strlen(key);
    unsigned long fhash=0, rhash = 0, hash=5381;
    int nt;
    for (i = 0; i<strlen(key); i++) {
        // printf("calc hash function");
        if (key[i] == 'A' || key[i] == 'a') {
            nt = 0;
        } else if (key[i] == 'C' || key[i] == 'c') {
            nt = 1;
        } else if (key[i] == 'G' || key[i] == 'g') {
            nt = 2;
        } else if (key[i] == 'T' || key[i] == 't') {
            nt = 3;
        }
        // hash += pow(4, k-j) * nt;
        // hash = hash * 4 ^ (k-j) * nt;
        hash = ((hash << 5) + hash) + nt;
        // printf("poly hash %u for nt %c\n", hash, key[i]);
        j--;
    }
    unsigned long new_hash = hash % CAPACITY;
    // printf("new hash %lu\n", new_hash);
    // return new_hash;
    return kh_str_hash_func(key) % CAPACITY;

    // return 0;
}
 
typedef struct Ht_item Ht_item;
 
// Define the Hash Table Item here
struct Ht_item {
    char* key;
    int value;
};
 
 
typedef struct LinkedList LinkedList;
 
// Define the Linkedlist here
struct LinkedList {
    Ht_item* item; 
    LinkedList* next;
};
 
 
typedef struct HashTable HashTable;
 
// Define the Hash Table here
struct HashTable {
    // Contains an array of pointers
    // to items
    Ht_item** items;
    LinkedList** overflow_buckets;
    int size;
    int count;
};

typedef struct ProcInfo ProcInfo;
struct ProcInfo {

    char* bamFileName;
    HashTable *ht;
};
 
 
static LinkedList* allocate_list () {
    // Allocates memory for a Linkedlist pointer
    LinkedList* list = (LinkedList*) malloc (sizeof(LinkedList));
    return list;
}
 
static LinkedList* linkedlist_insert(LinkedList* list, Ht_item* item) {
    // Inserts the item onto the Linked List
    if (!list) {
        LinkedList* head = allocate_list();
        head->item = item;
        head->next = NULL;
        list = head;
        return list;
    } 
     
    else if (list->next == NULL) {
        LinkedList* node = allocate_list();
        node->item = item;
        node->next = NULL;
        list->next = node;
        return list;
    }
 
    LinkedList* temp = list;
    while (temp->next->next) {
        temp = temp->next;
    }
     
    LinkedList* node = allocate_list();
    node->item = item;
    node->next = NULL;
    temp->next = node;
     
    return list;
}
 
static Ht_item* linkedlist_remove(LinkedList* list) {
    // Removes the head from the linked list
    // and returns the item of the popped element
    if (!list)
        return NULL;
    if (!list->next)
        return NULL;
    LinkedList* node = list->next;
    LinkedList* temp = list;
    temp->next = NULL;
    list = node;
    Ht_item* it = NULL;
    memcpy(temp->item, it, sizeof(Ht_item));
    free(temp->item->key);
    // free(temp->item->value);
    free(temp->item);
    free(temp);
    return it;
}
 
static void free_linkedlist(LinkedList* list) {
    LinkedList* temp = list;
    while (list) {
        temp = list;
        list = list->next;
        free(temp->item->key);
        // free(temp->item->value);
        free(temp->item);
        free(temp);
    }
}
 
static LinkedList** create_overflow_buckets(HashTable* table) {
    // Create the overflow buckets; an array of linkedlists
    LinkedList** buckets = (LinkedList**) calloc (table->size, sizeof(LinkedList*));
    int i;
    for (i=0; i<table->size; i++)
        buckets[i] = NULL;
    return buckets;
}
 
static void free_overflow_buckets(HashTable* table) {
    // Free all the overflow bucket lists
    LinkedList** buckets = table->overflow_buckets;
    int i;
    for (i=0; i<table->size; i++)
        free_linkedlist(buckets[i]);
    free(buckets);
}
 
 
Ht_item* create_item(char* key, int value) {
    // Creates a pointer to a new hash table item
    Ht_item* item = (Ht_item*) malloc (sizeof(Ht_item));
    item->key = (char*) malloc (strlen(key) + 1);
    item->value = (int) malloc (sizeof(int));
     
    strcpy(item->key, key);
    item->value = value;
 
    return item;
}
 
HashTable* create_table(int size) {
    // Creates a new HashTable
    HashTable* table = (HashTable*) malloc (sizeof(HashTable));
    table->size = size;
    table->count = 0;
    table->items = (Ht_item**) calloc (table->size, sizeof(Ht_item*));
    int i;
    for (i=0; i<table->size; i++)
        table->items[i] = NULL;
    table->overflow_buckets = create_overflow_buckets(table);
 
    return table;
}
 
void free_item(Ht_item* item) {
    // Frees an item
    free(item->key);
    // free(item->value);
    free(item);
}
 
void free_table(HashTable* table) {
    // Frees the table
    int i;
    for (i=0; i<table->size; i++) {
        Ht_item* item = table->items[i];
        if (item != NULL)
            free_item(item);
    }
 
    free_overflow_buckets(table);
    free(table->items);
    free(table);
}
 
void handle_collision(HashTable* table, unsigned long index, Ht_item* item) {
    LinkedList* head = table->overflow_buckets[index];
 
    if (head == NULL) {
        // We need to create the list
        head = allocate_list();
        head->item = item;
        table->overflow_buckets[index] = head;
        return;
    }
    else {
        // Insert to the list
        table->overflow_buckets[index] = linkedlist_insert(head, item);
        return;
    }
 }
 
void ht_insert(HashTable* table, char* key, int value) {
    // Create the item
    Ht_item* item = create_item(key, value);
    // printf("--------Kmer being inserted is  %s\n", key);
 
    // Compute the index
    unsigned long index = hash_function(key);
    // printf("the caklculated index is %lu\n", index);
 
    Ht_item* current_item = table->items[index];
     
    if (current_item == NULL) {
        // Key does not exist.
        if (table->count == table->size) {
            // Hash Table Full
            printf("Insert Error: Hash Table is full\n");
            // Remove the create item
            free_item(item);
            return;
        }
         
        // Insert directly
        table->items[index] = item; 
        table->count++;
    }
 
    else {
            // Scenario 1: We only need to update value
            if (strcmp(current_item->key, key) == 0) {
                // strcpy(table->items[index]->value, value);
                table->items[index]->value = value;
                return;
            }
     
        else {
            // Scenario 2: Collision
            handle_collision(table, index, item);
            return;
        }
    }
}
 
int ht_search(HashTable* table, char* key) {
    // Searches the key in the hashtable
    // and returns -1 if it doesn't exist
    // printf("> in search \n");
    int index = hash_function(key);
    // printf("Calculated index %d\n", index);
    Ht_item* item = table->items[index];
    LinkedList* head = table->overflow_buckets[index];
   
    // Ensure that we move to items which are not NULL
    while (item != NULL) {
        // printf("> in while \n");
        // printf("item key is %s\n", item->key);
        // printf("current key is %s\n", key);
        if (strcmp(item->key, key) == 0){
            // printf("> :):):):):)this key %s is found \n", key);
            return item->value;
        }
        if (head == NULL)
            return -1;
        item = head->item;
        head = head->next;
    }
    // printf("kmer %s doesn't exist\n", key);
    return -1;
}
 
int ht_increase_count(HashTable* table, char* key) {
    unsigned long index = hash_function(key);
    Ht_item* item = table->items[index];
    LinkedList* head = table->overflow_buckets[index];

    if (item == NULL) {
        // Does not exist. Return
        return -1;
    } else {
        while (item != NULL) {
            if (strcmp(item->key, key) == 0)
                item->value += 1;
                return 1;
            if (head == NULL)
                return -1;
            item = head->item;
            head = head->next;
        }
    return -1;
    }
}
void ht_delete(HashTable* table, char* key) {
    // Deletes an item from the table
    int index = hash_function(key);
    Ht_item* item = table->items[index];
    LinkedList* head = table->overflow_buckets[index];
 
    if (item == NULL) {
        // Does not exist. Return
        return;
    }
    else {
        if (head == NULL && strcmp(item->key, key) == 0) {
            // No collision chain. Remove the item
            // and set table index to NULL
            table->items[index] = NULL;
            free_item(item);
            table->count--;
            return;
        }
        else if (head != NULL) {
            // Collision Chain exists
            if (strcmp(item->key, key) == 0) {
                // Remove this item and set the head of the list
                // as the new item
                 
                free_item(item);
                LinkedList* node = head;
                head = head->next;
                node->next = NULL;
                table->items[index] = create_item(node->item->key, node->item->value);
                free_linkedlist(node);
                table->overflow_buckets[index] = head;
                return;
            }
 
            LinkedList* curr = head;
            LinkedList* prev = NULL;
             
            while (curr) {
                if (strcmp(curr->item->key, key) == 0) {
                    if (prev == NULL) {
                        // First element of the chain. Remove the chain
                        free_linkedlist(head);
                        table->overflow_buckets[index] = NULL;
                        return;
                    }
                    else {
                        // This is somewhere in the chain
                        prev->next = curr->next;
                        curr->next = NULL;
                        free_linkedlist(curr);
                        table->overflow_buckets[index] = head;
                        return;
                    }
                }
                curr = curr->next;
                prev = curr;
            }
 
        }
    }
}
 
void print_search(HashTable* table, char* key) {
    int val;
    if ((val = ht_search(table, key)) == -1) {
        // printf("%s does not exist\n", key);
        return;
    }
    else {
        printf("Key:%s, Value:%u\n", key, val);
    }
}
 
void print_table(HashTable* table) {
    printf("\n-------------------\n");
    int count = 0;
    int i;
    for (i=0; i<table->size; i++) {
        if (table->items[i]) {
            printf("Index:%d, Key:%s, Value:%u", i, table->items[i]->key, table->items[i]->value);
            count++;

            if (table->overflow_buckets[i]) {
                printf(" => Overflow Bucket => ");
                LinkedList* head = table->overflow_buckets[i];
                while (head) {
                    printf("Key:%s, Value:%u ", head->item->key, head->item->value);
                    count++;
                    head = head->next;
                }
            }
            printf("\n");
        }

    }
    printf("number of keys: %u ",count);
    printf("-------------------\n");
}

HashTable* generateHt(const char* kmersFileName) {
    HashTable* ht = create_table(CAPACITY);
    // printf(">> Created a hashtable\n");
    FILE* fs = fopen(kmersFileName, "r");
    if (fs== NULL) {
        exit(EXIT_FAILURE);
    }

    char* kmer = NULL;
    size_t len = 0;
    size_t nread=0;
    int count = 0;
    while ((nread=getline(&kmer, &len, fs)) != -1) {
        // need to strip the newline char in the end
        if (kmer[strlen(kmer) -1] == '\n') {
            kmer[strlen(kmer) -1] = '\0';
        }
        int i, l;
        if (ht_search(ht, kmer) != -1){
            printf("> kmer %s is DUPLICATED, please dedup first\n", kmer);
            return 0;
        } else {
            ht_insert(ht, kmer, 0);
            kmer_length = strlen(kmer);
            count++;
        }
    }
    // print_table(ht);
    // printf("Inserted  %d kmers to the hashtable\n", count);

    // finished reading kmer file
    fclose(fs);
    if (kmer) {
        free(kmer);
    }
    return ht;
}
// void scanBam(const char* bamFileName, int* nReads, bam_hdr_t *bamHdr, bam1_t *b){
//     htsFile *htsfp;
//     htsfp = hts_open(bamFileName,"r");
//     while (sam_read1(htsfp, bamHdr, b) > 0) {
//         nReads+=1;
//     }
// }
int main(int argc, char *argv[]) {
    clock_t before,after;
    double cpu_time_used;
    before = clock();
    char* bamFileName=argv[1];
    char* kmersFileName=argv[2];
    int num_kmers = 0;
    int nproc = (int)argv[3];
    if (argc != 4) {
        printf("Usage: gcc kmerScan.c -o kmerScan xx.bam nprocess kmersList.txt OR edit./runKmerScan.sh \n");
    }   
    HashTable *ht = generateHt(kmersFileName);

    htsFile *htsfp;
    htsfp = hts_open(bamFileName,"r");
    // first scan the bam to get the number of reads and other info
    bam_hdr_t *bamHdr;
    bamHdr = sam_hdr_read(htsfp);
    bam1_t *b;
    // scanBam(bamFileName, &nReads, &bam_hdr_t, &bamHdr, &b);


    // ////////////////// read the header of bam /////////////////////
    if ((b = bam_init1()) == NULL) {
        printf("Initialize bam failed\n");
        return 0;
    } 
    else {
        printf(" > Initialized new bam alignment\n");
    }

    // read index file
    hts_idx_t *idx;
    if ((idx = sam_index_load(htsfp, bamFileName)) == 0) {
        printf( "ERROR reading index\n");
        return 0;
    }
 
    const htsFormat *fmt = hts_get_format(htsfp);
    if (fmt == NULL || (fmt->format != sam && fmt->format != bam)) {
        printf("Cannot determine format of input reads.\n");
        return 0;
    }
    int start;
    int end;
	int time_taken;
    // uint32_t *tar = bamHdr->text;
    // uint32_t *tarlen = bamHdr->target_len;
    // uint32_t *ref_count = bamHdr->ref_count;

    // printf("the l_data is : %d\n",b->l_data);
    // printf("the m_data is : %d\n",b->m_data);
    // printf("the tar is : %d, the tarlen is: %d, num reference seq is %d\n",tar, tarlen, ref_count);
            
    // read bam reads
    int nReads=0;
	char* currChr;
    printf(" > Start searching the bam file\n");
    while (sam_read1(htsfp, bamHdr, b) > 0) {
        nReads+=1;

        //left most position of alignment in zero based coordianate (+1)
        int32_t pos = b->core.pos + 1; 

        //contig name (chromosome)
        char *chr = bamHdr->target_name[b->core.tid] ; 
		if (strlen(currChr)==0 || strcmp(chr, currChr)!=0) {
			time_taken = clock();
			cpu_time_used = ((double)(time_taken-before))/CLOCKS_PER_SEC;
			printf("kmerScan took %f seconds to execute %s\n", cpu_time_used, currChr);
			currChr = chr;
			printf("Processing %s\n", currChr);
		}
        //the read length
        uint32_t readLength = b->core.l_qseq; 

        //quality string
        uint8_t *bam_seq = bam_get_seq(b); 
        char *qseq = (char *)malloc(readLength);
        char *kseq = (char *)malloc(kmer_length+1);
        char *dest = (char *)malloc(kmer_length+1);

        uint32_t len = b->core.l_qseq; //length of the read.
        int i;
        for(i=0; i< len ; i++){
            qseq[i] = seq_nt16_str[bam_seqi(bam_seq,i)]; //gets nucleotide id and converts them into IUPAC id.
        }

        // slide a window of size k on the current read
        // int i;
        for (i=0; i < readLength-kmer_length+1; i++) {

            // get the kmer starting from i
            int k;
            for (k=0; k < kmer_length; k++) {
                kseq[k] = seq_nt16_str[bam_seqi(bam_seq,i+k)]; 
            }

            // search this kmer kseq in hahstable & update count
            strncpy(dest, kseq, kmer_length);
            if (ht_search(ht, dest)!=-1) {
                ht_increase_count(ht, dest);
            }
        }
        // printf(" %s\t%d\t%s\n",chr,readLength, qseq);
//        printf(" %s\t%d\t%d\n",chr,readLength, nReads);

    }

    // ProcInfo procInfo;
    // ProcInfo.bamFileName = bamFileName;
    // procInfo.ht = ht;
    // // vector<ProcInfo> procs(nproc);
    // pthread_t *threads = pthread_t[nproc];

    // pthread_attr_t *threadAttr = pthread_attr_t[nproc];
    // int int;
    // for procIndex = 0; procIndex < nproc; procIndex++ ){
    //         pthread_attr_init(&threadAttr[procIndex]);
    // }
    // int int;
    // for i = 0; i < nproc; i++) {
    //     pthread_create(&threads[i], &threadAttr[i], (void* (*)(void*)) , &procs[i]);
    // }
    // int int;
    // for procIndex = 0; procIndex < nproc; procIndex++) {
    //     pthread_join(threads[procIndex], NULL);
    // }
    // print_table(ht);
    after = clock();
    cpu_time_used = ((double)(after-before))/CLOCKS_PER_SEC;
    printf("kmerScan took %f seconds to execute \n", cpu_time_used); 
    free_table(ht);
    sam_hdr_destroy(bamHdr);
    bam_destroy1(b);
    hts_close(htsfp);

    return 0;
}
