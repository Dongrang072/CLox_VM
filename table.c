#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table *table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table *table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static Entry *findEntry(Entry *entries, int capacity, ObjString *key) {
    uint32_t index = key->hash % capacity; // %연산자로 키의 해시 코드를 배열 경계 내부의 인덱스에 매핑
    Entry* tombstone = NULL;
    for (;;) { //버킷에 엔트리가 있지만 키가 달라서 해시 충돌이 일어날 경우 -> probing
        Entry *entry = &entries[index];
            //엔트리 자체가 없고 엔트리를 삽입하는 용도로 함수를사용할 경우에는 새 엔트리를 추가할 위치를 찾았다는 뜻
            // 삽입을 하는 경우에는 새 엔트리를 추가하는 대신 찾은 키의 값을 바꿀 것이다.
        if(entries->key == NULL){
            if(IS_NIL(entries->value)){
                //엔트리를 비운다
                return tombstone != NULL ? tombstone : entry;
            } else {
                //툼스톤을 찾은 경우
                if(tombstone==NULL) tombstone = entry;
            }
        } else if (entry->key == key){
            return entry;
        }
        index = (index + 1) % capacity; //배열 끝을 넘어가면 두 번째 나머지 연산자가 처음으로 되돌린다.
    }

}

bool tableGet(Table* table, ObjString* key, Value* value){
    if(table->count ==0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if(entry->key == NULL) return false;

    *value =entry->value;
    return true;
}

static void adjustCapacity(Table *table, int capacity) { // 버킷 배열 할당
    Entry *entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    //이미 배열이 있는 상태에서 reallocate 동적할당을 하는 대신에(해시 키를 배열 크기로 나눈 나머지를 계산하므로)
    //처음부터 배열을 다시 만들어서 엔트리를 다시 새로운 빈 배열에 삽입
    for (int i = 0; i < table->capacity; i++) {
        Entry *entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry *dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++; //툼스톤이 아닌 앤트리가 나올 때 마다 증가
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(Table *table, ObjString *key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) { //Entry 배열 할당 전에 배열이 존재하는지 확인, 혹은 크기가 충분한지 확인하기
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }
    //해당 키로 매핑된 엔트리가 존재하면 새 값으로 이전 값을 덮어씌운다
    Entry *entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && IS_NIL(entry->value)) table->count++; //완전히 빈 버킷에 새 엔트리가 들어갈 때만 count++(count == 앤트리 수 + 툼스톤 수)

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table* table, ObjString* key){
    if(table->count ==0) return false;

    //엔트리를 찾는다
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if(entry->key == NULL) return false;

    //툼스톤 삽입
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void tableAddAll(Table *from, Table *to){
    for(int i =0; i < from->capacity; i++){
        Entry* entry =&from->entries[i];
        if(entry->key !=NULL ){
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjString* tableFindString(Table* table , const char* chars, int length, uint32_t hash){
    if(table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;

    for(;;){
        Entry* entry = &table->entries[index];
        if(entry->key == NULL) {
            //툼스톤이 아닌 엔트리가 나올 경우 멈춘다
            if(IS_NIL(entry->value)) return NULL;
        } else if(entry->key->length == length && entry->key->hash == hash &&
                memcpy(entry->key->chars, chars, length) == 0){
            //찾은 경우
            return entry->key;
        }

        index = (index +1) % table->capacity;
    }

}