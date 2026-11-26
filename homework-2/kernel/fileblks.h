struct fileblks {
    int blocks[12 + 128]; // NDIRECT + NINDIRECT
    int num_blocks; // Ukupan broj zauzetih blokova
    int last_block_free; // Preostalo bajtova u poslednjem bloku
};
