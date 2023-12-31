#include "../include/Distribute.h"

static TShadow** initializeShadows(TShadowGenerator* shadowGenerator, uint32_t blockCount);
static uint8_t poly(uint8_t k, uint8_t* coefficients, uint8_t value);
static void distributeSecret(TShadowGenerator* shadowGenerator);
static TShadowGenerator* initializeDistributor(TParams* params);
static void hideSecret(TShadowGenerator * shadowGenerator);
static void hideShadow(uint8_t  k , bmpFile * image, TShadow * hidingShadow);
static void insertBits(uint8_t  *imagePixelPointer, uint8_t  *shadowPointer, uint8_t k);

uint8_t fourSignificant[] = {0xC0, 0x30, 0x0C, 0x03};
uint8_t twoSignificant[] = {0xF0, 0x0F};


void distribute(TParams* params) {
    TShadowGenerator* generator = initializeDistributor(params);
    distributeSecret(generator);
    hideSecret(generator);

    for (int i = 0; i < generator->n; i++) {
        free(generator->imageFiles[i]);
    }
    
    free(generator->imageFiles);
    free(generator->file->header);
    free(generator->file);
    freeShadows(generator->generatedShadows, generator->n);
    free(generator);
}

//---------------------------------------------------
// STATIC FUNCTIONS ---------------------------------
//---------------------------------------------------

static TShadowGenerator* initializeDistributor(TParams* params) {
    TShadowGenerator* shadowGenerator = malloc(sizeof(TShadowGenerator));
    shadowGenerator->file = openBmpFile(params->file);
    shadowGenerator->k = params->k;
    shadowGenerator->n = params->n;
    openDirectory(shadowGenerator, params->directory);
    return shadowGenerator;
}

static void distributeSecret(TShadowGenerator* shadowGenerator) {

    uint32_t blockCount = (shadowGenerator->file->header->image_size_bytes) / (shadowGenerator->k - 1);
    uint8_t k = shadowGenerator->k;
    uint8_t blockSize = 2 * k - 2;

    TShadow** shadows = initializeShadows(shadowGenerator, blockCount);

    uint8_t* blockPosition = shadowGenerator->file->pixels;
    
    uint32_t currentBlock = 0;
    uint8_t a_0, a_1;

    while (currentBlock < blockCount) {
        uint8_t* a_c = malloc(k * sizeof(uint8_t));
        uint8_t* b_c = malloc(k * sizeof(uint8_t));
        if (a_c == NULL || b_c == NULL){
            exitError(ERROR_MALLOC);
        }

        memcpy(a_c, blockPosition, k);
        memcpy(b_c + 2, blockPosition + k, k - 2);
        
        uint8_t r = 0;
        while (r == 0) {
            r = rand() % 251;
        }
        a_0 = mod(a_c[0]) == 0 ? 1 : a_c[0];
        a_1 = mod(a_c[1]) == 0 ? 1 : a_c[1];
        b_c[0] = mod(times(mod(-r), a_0));
        b_c[1] = mod(times(mod(-r), a_1));

        for (int j = 0; j < shadowGenerator->n; j++) {
            shadows[j]->values[currentBlock] = poly(shadowGenerator->k,
                a_c, shadows[j]->shadowNumber);
            shadows[j]->values[currentBlock + 1] = poly(shadowGenerator->k,
                b_c, shadows[j]->shadowNumber);
        }

        blockPosition += blockSize;
        currentBlock += 2;
        free(a_c);
        free(b_c);
    }
    shadowGenerator->generatedShadows = shadows;
}

static TShadow ** initializeShadows(TShadowGenerator* shadowGenerator, uint32_t blockCount) {

    TShadow** shadows = malloc(shadowGenerator->n * sizeof(TShadow*));
    
    //initialize all TShadow structures.
    for (int i = 0; i < shadowGenerator->n; i++) {
        shadows[i] = malloc(sizeof(TShadow));
        if (shadows[i] == NULL){
            freeShadows(shadows, shadowGenerator->n);
            exitError(ERROR_MALLOC);
            return NULL;
        }
        shadows[i]->shadowNumber = i + 1;
        shadows[i]->valuesSize = blockCount;
        shadows[i]->values = malloc(blockCount * sizeof(uint8_t));
        if (shadows[i]->values == NULL){
            freeShadows(shadows, shadowGenerator->n);
            exitError(ERROR_MALLOC);
            return NULL;
        }
    }
    return  shadows;
}

static uint8_t poly(uint8_t k, uint8_t* coefficients, uint8_t value) {
    uint8_t result = 0;
    uint8_t exp = 1;

    uint8_t x2 = mod(value);

    for (uint8_t i = 0; i < k; i++) {
        result = sum(result, times(coefficients[i], exp));
        exp = times(exp, x2);
    }

    return result;
}

static void hideSecret(TShadowGenerator * shadowGenerator){
    for (int i = 0 ; i < shadowGenerator -> n ; i ++){
        bmpFile  * currentImageFile = openBmpFile(shadowGenerator->imageFiles[i]);
        TShadow * currentShadow = shadowGenerator->generatedShadows[i];
        hideShadow(shadowGenerator->k,currentImageFile, currentShadow);

        //save the generated image.
        int headerSize = currentImageFile->header->size - currentImageFile->header->image_size_bytes;
        //re-write the entire file.
        lseek(currentImageFile->fd, 0, SEEK_SET);
        write(currentImageFile->fd , currentImageFile->header, headerSize);
        write(currentImageFile->fd , currentImageFile->pixels, currentImageFile->header->image_size_bytes);
        close(currentImageFile->fd);

        free(currentImageFile->header);
        free(currentImageFile);
    }

}


static void hideShadow(uint8_t  k , bmpFile * image, TShadow * hidingShadow){
    image->header->reserved1 = hidingShadow->shadowNumber; //save the shadow number.
    uint8_t * imagePixelPointer = image->pixels;
    uint8_t * shadowPointer = hidingShadow->values;
    for(uint32_t  i = 0; i < hidingShadow->valuesSize; i++ ){
            insertBits(imagePixelPointer, shadowPointer, k);
            shadowPointer += 1 ; //go to the next point;
            imagePixelPointer += (k == 3 || k == 4) ? 2 : 4;
    }
}

static void insertBits(uint8_t  * imagePixelPointer, uint8_t  *shadowPointer, uint8_t  k){
    int lsb4 = ( k == 3 || k == 4 ) ? 1 : 0;
    int bytesUsedFromImage = ( lsb4 ) ? 2 : 4;

    uint8_t  lsb4Shifter[] = {4, 0};
    uint8_t  lsb2Shifter[] = {6,4,2, 0};
    int currentShifterIndex = 0 ;

    uint8_t  bits[bytesUsedFromImage];

    for(int i = 0; i < bytesUsedFromImage ; i++){
        bits[i] = lsb4 ? *shadowPointer & twoSignificant[i] : *shadowPointer & fourSignificant[i] ;
        bits[i] = lsb4 ? bits[i] >> lsb4Shifter[currentShifterIndex] : bits[i] >>lsb2Shifter[currentShifterIndex];
        currentShifterIndex++;
    }

    int and = lsb4 ? 0xF0 : 0xFC;  // 4 o 6 MSB.
    for (int i = 0 ; i < bytesUsedFromImage ; i++)
        imagePixelPointer[i] = (imagePixelPointer[i] & and) + bits[i];

}
