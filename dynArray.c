#include "dynArray.h"


/**
 * Erzeugt ein neues Array auf dem Heap-Speicher und legt
 * die anfängliche Item-Kapazität fest.
 *
 * @param capacity - Item-Kapazität
 */
Array* arrayCreateWithCapacity (size_t capacity)
{
    Array *arr = malloc(sizeof(Array));

    arr->cArr = malloc(capacity * sizeof(void*));
    arr->capacity = capacity;
    arr->size = 0;

    return arr;
}


/**
 * Erzeugt ein neues Array auf dem Heap-Speicher
 *
 */
Array* arrayCreate ()
{
    return arrayCreateWithCapacity(INITIAL_ARRAY_CAPACITY);
}


/**
 * Erzeugt ein neues Array auf dem Heap-Speicher und initialisiert es
 * mit einem C-Style Array.
 *
 * @param size - Anzahl der Elemente in cArr
 * @param cArr - Initialisierungswerte
 */
Array* arrayCreateWithArray (size_t size, void* cArr[])
{
    Array *arr = arrayCreateWithCapacity(size);
    memcpy(arr->cArr, cArr, size * sizeof(void*));
    return arr;
}


/**
 * Erzeugt ein neues Array auf dem Heap-Speicher und initialisiert es
 * mit den Funktions-Argumenten.
 *
 * @param size - Anzahl der Initialisierungswerte
 * @param ... - Initialisierungswerte
 */
Array* arrayCreateWithArguments (size_t size, ...)
{
    Array *arr = arrayCreateWithCapacity(size);
    arr->size = size;

    va_list vaList;

    va_start(vaList, size);
    for (int i = 0; i < size; i++) {
        void *ptr = va_arg(vaList, void*);
        arr->cArr[i] = ptr;
    };
    va_end(vaList);

    return arr;
}


/**
 * Gibt dem Heap-Speicher des Arrays wieder frei.
 *
 * @param arr - Zielobjekt
 */
void arrayFree (Array *arr)
{
    free(arr->cArr);
    free(arr);
}


/**
 * Verändert die Kapazität auf dem Heap-Speicher.
 * 'capacity' entspricht der Anzahl der Items die das Zielobjekt fassen kann.
 *
 * @param arr - Zielobjekt
 * @param capacity - Item-Kapazität
 */
void arrayReserve (Array *arr, size_t capacity)
{
    arr->cArr = realloc(arr->cArr, capacity * sizeof(void*));
    arr->capacity = capacity;
}


/**
 * Reduziert die Kapazität des Heap-Speicher auf die Anzahl der Items
 * des Zielobjekts.
 *
 * @param arr - Zielobjekt
 */
void arrayShrinkToFit (Array* arr)
{
    arrayReserve(arr, arraySize(arr));
}


/**
 * Gibt die Anzahl der Items zurück.
 *
 * @param arr - Zielobjekt
 * @return Anzahl der Items
 */
size_t arraySize (const Array *arr)
{
    return arr->size;
}


/**
 * Wahr wenn das Array keine Items beinhaltet.
 *
 * @param arr - Zielobjekt
 */
bool arrayIsEmpty (const Array *arr)
{
    return arr->size == 0;
}


/**
 * Entfernt alle Items aus dem Array.
 *
 * @param arr - Zielobjekt
 */
void arrayClear (Array *arr)
{
    arr->size = 0;
}


void* arrayGetItem (const Array *arr, size_t index)
{
    return arr->cArr[index];
}


void arraySetItem (Array *arr, size_t index, void *item)
{
    arr->cArr[index] = item;
}


/**
 * Fügt ein Item am Ende des Arrays ein.
 *
 * @param arr - Zielobjekt
 * @param item - Einzufügendes Item
 */
void arrayPushItem (Array *arr, void *item)
{
    if (arr->size == arr->capacity) {
        int newCapacity = arr->capacity * 2;
        if (INITIAL_ARRAY_CAPACITY > newCapacity) {
            newCapacity = INITIAL_ARRAY_CAPACITY;
        }
        arrayReserve(arr, newCapacity);
    }

    arr->cArr[arr->size++] = item;
}


/**
 * Entfernt ein Item am Ende des Arrays and gibt es zurück.
 * Wenn kein Item im Array ist, ist der Rückgabewert NULL.
 *
 * @param arr - Zielobjekt
 * @return - Entferntes Item
 */
void* arrayPopItem (Array *arr)
{
    if (arr->size == 0) return NULL;
    return arr->cArr[--arr->size];
}


/**
 * Verschiebt ein Item im Array von Index "src" zu Index "dest".
 *
 * @param arr - Zielobjekt
 * @param src - Ursprungs-Index
 * @param dest - Ziel-Index
 */
void arrayShiftItem (Array *arr, size_t src, size_t dest)
{
    void *item = arr->cArr[src];

    if (src < dest) {
        for (int i = src; i < dest; i++) {
            arr->cArr[i] = arr->cArr[i+1];
        }
    }
    else if (src > dest) {
        for (int i = src; i > dest; i--) {
            arr->cArr[i] = arr->cArr[i-1];
        }
    }

    arr->cArr[dest] = item;
}


/**
 * Fügt ein Item an eine bestimmte Position ein und verschiebt alle
 * nachfolgenden Items noch hinten.
 *
 * @param arr - Zielobjekt
 * @param index - Ziel-Index
 * @param item - Einzufügendes Item
 */
void arrayInsertItem (Array *arr, size_t index, void *item)
{
    arrayPushItem(arr, item);

    if (arr->size > 0 && arr->size > index) {
        arrayShiftItem(arr, arr->size - 1, index);
    }
}


/**
 * Entfernt ein Item aus einer bestimmten Position und verschiebt
 * alle nachfolgenden Items nach vorne.
 *
 * @param arr - Zielobjekt
 * @param index - Ziel-Index
 * @return - Entferntes Item
 */
void* arrayRemoveItem (Array *arr, size_t index)
{
    if (arr->size > 0 && arr->size > index) {
        arrayShiftItem(arr, index, arr->size - 1);
    }

    return arrayPopItem(arr);
}


void arrayForEach (Array *arr, void (*func)(void*)) {
    for (int i = 0; i < arr->size; i++) {
        func(arr->cArr[i]);
    }
}

