#ifndef HASH_H
#define HASH_H



#ifdef TESTING
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;

struct dummy
{ dword id; };

struct window_list
{
  dummy *self;

  window_list(dword id)
  {
    self= (dummy *)malloc(sizeof(dummy));
    self->id= id;
  }

  ~window_list()
  {
    free(self);
  }
};
#endif  // TESTING





template<class T2> struct hash_item
{
  dword id;
  T2 *value;
};

template<> struct hash_item<window_list>
{
  window_list *value;
};


template<class T> struct int_hash
{
  private:
    int max_items;
    int n_items;
    typedef struct hash_item<T> hash_item_t;
    hash_item_t *items;

    int lookup_idx(dword id);
    inline void setitem(int idx, dword id, T *value);
    bool realloc(int n);
    bool reinsert(int oldmax_items, hash_item_t *olditems);


  public:
#ifdef STATS
    int stat_coll_add;
    int stat_coll_lookup;
#endif

//    int_hash();
//    ~int_hash();


    // Alles virtual wegen den bloeden Templates
    virtual bool construct();
    virtual bool destroy();

    int size() { return n_items; }

    virtual int add(dword id, T *value);
    virtual int del(dword id);
    virtual T *lookup(dword id);

    virtual void enum_items( void (*callback)(int_hash<T> *hash, T *item) );
};



#endif //HASH_H
