<?php

/* Persistent connection to localhost */
$tt = new TokyoTyrant("localhost", 1978, true);

// put a record 
$tt->put("hello", "hello");

// concat to the record 
$tt->putcat("hello", " world");

// get it back, should be 'hello world'
var_dump($tt->get("hello"));

// put multiple 
$tt->put(array("key1" => "value1", "key2" => "value2"));

// get multiple
var_dump($tt->get(array("key1", "key2")));
