typedef union {
  OZ_Term t;
  int i;
} YYSTYPE;
#define	T_SWITCH	257
#define	T_SWITCHNAME	258
#define	T_LOCALSWITCHES	259
#define	T_PUSHSWITCHES	260
#define	T_POPSWITCHES	261
#define	T_OZATOM	262
#define	T_ATOM_LABEL	263
#define	T_OZFLOAT	264
#define	T_OZINT	265
#define	T_AMPER	266
#define	T_DOTINT	267
#define	T_STRING	268
#define	T_VARIABLE	269
#define	T_VARIABLE_LABEL	270
#define	T_DEFAULT	271
#define	T_CHOICE	272
#define	T_LDOTS	273
#define	T_2DOTS	274
#define	T_attr	275
#define	T_at	276
#define	T_case	277
#define	T_catch	278
#define	T_choice	279
#define	T_class	280
#define	T_cond	281
#define	T_declare	282
#define	T_define	283
#define	T_dis	284
#define	T_else	285
#define	T_elsecase	286
#define	T_elseif	287
#define	T_elseof	288
#define	T_end	289
#define	T_export	290
#define	T_fail	291
#define	T_false	292
#define	T_FALSE_LABEL	293
#define	T_feat	294
#define	T_finally	295
#define	T_from	296
#define	T_fun	297
#define	T_functor	298
#define	T_if	299
#define	T_import	300
#define	T_in	301
#define	T_local	302
#define	T_lock	303
#define	T_meth	304
#define	T_not	305
#define	T_of	306
#define	T_or	307
#define	T_prepare	308
#define	T_proc	309
#define	T_prop	310
#define	T_raise	311
#define	T_require	312
#define	T_self	313
#define	T_skip	314
#define	T_then	315
#define	T_thread	316
#define	T_true	317
#define	T_TRUE_LABEL	318
#define	T_try	319
#define	T_unit	320
#define	T_UNIT_LABEL	321
#define	T_for	322
#define	T_FOR	323
#define	T_do	324
#define	T_ENDOFFILE	325
#define	T_REGEX	326
#define	T_lex	327
#define	T_mode	328
#define	T_parser	329
#define	T_prod	330
#define	T_scanner	331
#define	T_syn	332
#define	T_token	333
#define	T_REDUCE	334
#define	T_SEP	335
#define	T_ITER	336
#define	T_OOASSIGN	337
#define	T_DOTASSIGN	338
#define	T_orelse	339
#define	T_andthen	340
#define	T_COMPARE	341
#define	T_FDCOMPARE	342
#define	T_LMACRO	343
#define	T_RMACRO	344
#define	T_FDIN	345
#define	T_ADD	346
#define	T_FDMUL	347
#define	T_OTHERMUL	348
#define	T_DEREFF	349


extern YYSTYPE xylval;