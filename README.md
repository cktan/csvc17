### CSV Parser Library (C/C++)

This library provides efficient parsing of CSV documents. Key features include:

- **Stream Processing**: Content is read via a user-defined `feed` callback function.
- **Row Notification**: The library invokes a `perrow` callback function upon successfully parsing each row.
- **High-Performance Parsing**: Leverages SIMD instructions to rapidly scan for special characters (e.g., delimiters, quotes), significantly improving parsing speed.
- **C++ RAII Support**: Includes a C++ interface designed with Resource Acquisition Is Initialization (RAII) principles for safe resource management.

### Usage


``` c
/*
 *  Print the content of a csv file.
 */
typedef struct context_t context_t;
struct context_t {
	int count;
};

static int perrow(void* ctx, int n, csv_value_t value[], 
                  int64_t lineno, int64_t rowno, 
				  char* errbuf, int errsz) {
	context_t* context = ctx;
	context->count++;
	for (int i = 0; i < n; i++) {
	    printf("%s%s", (i ? ", " : ""), value[i].ptr);
    }
	printf("\n");
	return 0;
}


int main() {
	context_t context = {0};
    csv_config_t conf = csv_default_config();
    conf.delim = '|';
    csv_t csv = csv_open(&conf);
    if (!csv.ok) {
        ERROR(csv.errmsg);
    }
    if (csv_parse_file_ex(&csv, 'file.csv', context, perrow)) {
       ERROR(csv.errmsg);
    }
    csv_close(&csv);
	print("%d row(s)\n", context.count);
	return 0;
}	
```


``` c++
struct parser_t : csv_parser_t {
	int count;
}

static int perrow(void* ctx, int n, csv_value_t value[], 
                  int64_t lineno, int64_t rowno, 
				  char* errbuf, int errsz) {
	parser_t* p = (parser_t*) ctx;
	p->count++;
	for (int i = 0; i < n; i++) {
	    printf("%s%s", (i ? ", " : ""), value[i].ptr);
    }
	printf("\n");
	return 0;
}


int main() {
	parser_t p;
	p.set_delim('|');
	if (!p.parse_file('file.csv', perrow)) {
	    ERROR(p.errmsg());
	}	
	print("%d row(s)\n", p.count);
	return 0;
}	

```

## Building

For debug build:
```bash
export DEBUG=1
make
```

For release build:
```bash
unset DEBUG
make
```

## Running tests

The following command invokes the tests:

```bash
make test
```

## Installing

The install command will copy `csvc17.h`, `csv.hpp` and `libcsvc17.a`
to the `$prefix/include` and `$prefix/lib` directories.

```bash
unset DEBUG
make clean install prefix=/usr/local
```
