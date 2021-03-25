/***************************************************************************/
/*                                                                         */
/*  ttload.c                                                               */
/*                                                                         */
/*    Load the basic TrueType tables, i.e., tables that can be either in   */
/*    TTF or OTF fonts (body).                                             */
/*                                                                         */
/*  Copyright 1996-2001, 2002, 2003, 2004, 2005, 2006, 2007 by             */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#include <freetype/ft2build.h>
#include FT_INTERNAL_DEBUG_H
#include FT_INTERNAL_STREAM_H
#include FT_TRUETYPE_TAGS_H
#include "ttload.h"

#include "sferrors.h"


  /*************************************************************************/
  /*                                                                       */
  /* The macro FT_COMPONENT is used in trace mode.  It is an implicit      */
  /* parameter of the FT_TRACE() and FT_ERROR() macros, used to print/log  */
  /* messages during execution.                                            */
  /*                                                                       */
#undef  FT_COMPONENT
#define FT_COMPONENT  trace_ttload


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_lookup_table                                               */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Looks for a TrueType table by name.                                */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face :: A face object handle.                                      */
  /*                                                                       */
  /*    tag  :: The searched tag.                                          */
  /*                                                                       */
  /* <Return>                                                              */
  /*    A pointer to the table directory entry.  0 if not found.           */
  /*                                                                       */
  FT_LOCAL_DEF( TT_Table  )
  tt_face_lookup_table( TT_Face   face,
                        FT_ULong  tag  )
  {
    TT_Table  entry;
    TT_Table  limit;


    FT_TRACE4(( "tt_face_lookup_table: %08p, `%c%c%c%c' -- ",
                face,
                (FT_Char)( tag >> 24 ),
                (FT_Char)( tag >> 16 ),
                (FT_Char)( tag >> 8  ),
                (FT_Char)( tag       ) ));

    entry = face->dir_tables;
    limit = entry + face->num_tables;

    for ( ; entry < limit; entry++ )
    {
      /* For compatibility with Windows, we consider 0-length */
      /* tables the same as missing tables.                   */
      if ( entry->Tag == tag && entry->Length != 0 )
      {
        FT_TRACE4(( "found table.\n" ));
        return entry;
      }
    }

    FT_TRACE4(( "could not find table!\n" ));
    return 0;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_goto_table                                                 */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Looks for a TrueType table by name, then seek a stream to it.      */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face   :: A face object handle.                                    */
  /*                                                                       */
  /*    tag    :: The searched tag.                                        */
  /*                                                                       */
  /*    stream :: The stream to seek when the table is found.              */
  /*                                                                       */
  /* <Output>                                                              */
  /*    length :: The length of the table if found, undefined otherwise.   */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  FT_LOCAL_DEF( FT_Error )
  tt_face_goto_table( TT_Face    face,
                      FT_ULong   tag,
                      FT_Stream  stream,
                      FT_ULong*  length )
  {
    TT_Table  table;
    FT_Error  error;


    table = tt_face_lookup_table( face, tag );
    if ( table )
    {
      if ( length )
        *length = table->Length;

      if ( FT_STREAM_SEEK( table->Offset ) )
       goto Exit;
    }
    else
      error = SFNT_Err_Table_Missing;

  Exit:
    return error;
  }


  /* Here, we                                                              */
  /*                                                                       */
  /* - check that `num_tables' is valid                                    */
  /* - look for a `head' table, check its size, and parse it to check      */
  /*   whether its `magic' field is correctly set                          */
  /*                                                                       */
  /* When checking directory entries, ignore the tables `glyx' and `locx'  */
  /* which are hacked-out versions of `glyf' and `loca' in some PostScript */
  /* Type 42 fonts, and which are generally invalid.                       */
  /*                                                                       */
  static FT_Error
  check_table_dir( SFNT_Header  sfnt,
                   FT_Stream    stream )
  {
    FT_Error        error;
    FT_UInt         nn;
    FT_UInt         has_head = 0, has_sing = 0, has_meta = 0;
    FT_ULong        offset = sfnt->offset + 12;

    const FT_ULong  glyx_tag = FT_MAKE_TAG( 'g', 'l', 'y', 'x' );
    const FT_ULong  locx_tag = FT_MAKE_TAG( 'l', 'o', 'c', 'x' );

    static const FT_Frame_Field  table_dir_entry_fields[] =
    {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  TT_TableRec

      FT_FRAME_START( 16 ),
        FT_FRAME_ULONG( Tag ),
        FT_FRAME_ULONG( CheckSum ),
        FT_FRAME_ULONG( Offset ),
        FT_FRAME_ULONG( Length ),
      FT_FRAME_END
    };


    if ( sfnt->num_tables == 0                         ||
         offset + sfnt->num_tables * 16 > stream->size )
      return SFNT_Err_Unknown_File_Format;

    if ( FT_STREAM_SEEK( offset ) )
      return error;

    for ( nn = 0; nn < sfnt->num_tables; nn++ )
    {
      TT_TableRec  table;


      if ( FT_STREAM_READ_FIELDS( table_dir_entry_fields, &table ) )
        return error;

      if ( table.Offset + table.Length > stream->size &&
           table.Tag != glyx_tag                      &&
           table.Tag != locx_tag                      )
        return SFNT_Err_Unknown_File_Format;

      if ( table.Tag == TTAG_head || table.Tag == TTAG_bhed )
      {
        FT_UInt32  magic;


#ifndef TT_CONFIG_OPTION_EMBEDDED_BITMAPS
        if ( table.Tag == TTAG_head )
#endif
          has_head = 1;

        /*
         * The table length should be 0x36, but certain font tools make it
         * 0x38, so we will just check that it is greater.
         *
         * Note that according to the specification, the table must be
         * padded to 32-bit lengths, but this doesn't apply to the value of
         * its `Length' field!
         *
         */
        if ( table.Length < 0x36 )
          return SFNT_Err_Unknown_File_Format;

        if ( FT_STREAM_SEEK( table.Offset + 12 ) ||
             FT_READ_ULONG( magic )              )
          return error;

        if ( magic != 0x5F0F3CF5UL )
          return SFNT_Err_Unknown_File_Format;

        if ( FT_STREAM_SEEK( offset + ( nn + 1 ) * 16 ) )
          return error;
      }
      else if ( table.Tag == TTAG_SING )
        has_sing = 1;
      else if ( table.Tag == TTAG_META )
        has_meta = 1;
    }

    /* if `sing' and `meta' tables are present, there is no `head' table */
    if ( has_head || ( has_sing && has_meta ) )
      return SFNT_Err_Ok;
    else
      return SFNT_Err_Unknown_File_Format;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_load_font_dir                                              */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Loads the header of a SFNT font file.                              */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face       :: A handle to the target face object.                  */
  /*                                                                       */
  /*    stream     :: The input stream.                                    */
  /*                                                                       */
  /* <Output>                                                              */
  /*    sfnt       :: The SFNT header.                                     */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  /* <Note>                                                                */
  /*    The stream cursor must be at the beginning of the font directory.  */
  /*                                                                       */
  FT_LOCAL_DEF( FT_Error )
  tt_face_load_font_dir( TT_Face    face,
                         FT_Stream  stream )
  {
    SFNT_HeaderRec  sfnt;
    FT_Error        error;
    FT_Memory       memory = stream->memory;
    TT_TableRec*    entry;
    TT_TableRec*    limit;

    static const FT_Frame_Field  offset_table_fields[] =
    {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  SFNT_HeaderRec

      FT_FRAME_START( 8 ),
        FT_FRAME_USHORT( num_tables ),
        FT_FRAME_USHORT( search_range ),
        FT_FRAME_USHORT( entry_selector ),
        FT_FRAME_USHORT( range_shift ),
      FT_FRAME_END
    };


    FT_TRACE2(( "tt_face_load_font_dir: %08p\n", face ));

    /* read the offset table */

    sfnt.offset = FT_STREAM_POS();

    if ( FT_READ_ULONG( sfnt.format_tag )                    ||
         FT_STREAM_READ_FIELDS( offset_table_fields, &sfnt ) )
      return error;

    /* many fonts don't have these fields set correctly */
#if 0
    if ( sfnt.search_range != 1 << ( sfnt.entry_selector + 4 )        ||
         sfnt.search_range + sfnt.range_shift != sfnt.num_tables << 4 )
      return SFNT_Err_Unknown_File_Format;
#endif

    /* load the table directory */

    FT_TRACE2(( "-- Tables count:   %12u\n",  sfnt.num_tables ));
    FT_TRACE2(( "-- Format version: %08lx\n", sfnt.format_tag ));

    /* check first */
    error = check_table_dir( &sfnt, stream );
    if ( error )
    {
      FT_TRACE2(( "tt_face_load_font_dir: invalid table directory!\n" ));

      return error;
    }

    face->num_tables = sfnt.num_tables;
    face->format_tag = sfnt.format_tag;

    if ( FT_QNEW_ARRAY( face->dir_tables, face->num_tables ) )
      return error;

    if ( FT_STREAM_SEEK( sfnt.offset + 12 )       ||
         FT_FRAME_ENTER( face->num_tables * 16L ) )
      return error;

    entry = face->dir_tables;
    limit = entry + face->num_tables;

    for ( ; entry < limit; entry++ )
    {
      entry->Tag      = FT_GET_TAG4();
      entry->CheckSum = FT_GET_ULONG();
      entry->Offset   = FT_GET_LONG();
      entry->Length   = FT_GET_LONG();

      FT_TRACE2(( "  %c%c%c%c  -  %08lx  -  %08lx\n",
                  (FT_Char)( entry->Tag >> 24 ),
                  (FT_Char)( entry->Tag >> 16 ),
                  (FT_Char)( entry->Tag >> 8  ),
                  (FT_Char)( entry->Tag       ),
                  entry->Offset,
                  entry->Length ));
    }

    FT_FRAME_EXIT();

    FT_TRACE2(( "table directory loaded\n\n" ));

    return error;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_load_any                                                   */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Loads any font table into client memory.                           */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face   :: The face object to look for.                             */
  /*                                                                       */
  /*    tag    :: The tag of table to load.  Use the value 0 if you want   */
  /*              to access the whole font file, else set this parameter   */
  /*              to a valid TrueType table tag that you can forge with    */
  /*              the MAKE_TT_TAG macro.                                   */
  /*                                                                       */
  /*    offset :: The starting offset in the table (or the file if         */
  /*              tag == 0).                                               */
  /*                                                                       */
  /*    length :: The address of the decision variable:                    */
  /*                                                                       */
  /*                If length == NULL:                                     */
  /*                  Loads the whole table.  Returns an error if          */
  /*                  `offset' == 0!                                       */
  /*                                                                       */
  /*                If *length == 0:                                       */
  /*                  Exits immediately; returning the length of the given */
  /*                  table or of the font file, depending on the value of */
  /*                  `tag'.                                               */
  /*                                                                       */
  /*                If *length != 0:                                       */
  /*                  Loads the next `length' bytes of table or font,      */
  /*                  starting at offset `offset' (in table or font too).  */
  /*                                                                       */
  /* <Output>                                                              */
  /*    buffer :: The address of target buffer.                            */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  FT_LOCAL_DEF( FT_Error )
  tt_face_load_any( TT_Face    face,
                    FT_ULong   tag,
                    FT_Long    offset,
                    FT_Byte*   buffer,
                    FT_ULong*  length )
  {
    FT_Error   error;
    FT_Stream  stream;
    TT_Table   table;
    FT_ULong   size;


    if ( tag != 0 )
    {
      /* look for tag in font directory */
      table = tt_face_lookup_table( face, tag );
      if ( !table )
      {
        error = SFNT_Err_Table_Missing;
        goto Exit;
      }

      offset += table->Offset;
      size    = table->Length;
    }
    else
      /* tag == 0 -- the user wants to access the font file directly */
      size = face->root.stream->size;

    if ( length && *length == 0 )
    {
      *length = size;

      return SFNT_Err_Ok;
    }

    if ( length )
      size = *length;

    stream = face->root.stream;
    /* the `if' is syntactic sugar for picky compilers */
    if ( FT_STREAM_READ_AT( offset, buffer, size ) )
      goto Exit;

  Exit:
    return error;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_load_generic_header                                        */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Loads the TrueType table `head' or `bhed'.                         */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face   :: A handle to the target face object.                      */
  /*                                                                       */
  /*    stream :: The input stream.                                        */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  static FT_Error
  tt_face_load_generic_header( TT_Face    face,
                               FT_Stream  stream,
                               FT_ULong   tag )
  {
    FT_Error    error;
    TT_Header*  header;

    static const FT_Frame_Field  header_fields[] =
    {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  TT_Header

      FT_FRAME_START( 54 ),
        FT_FRAME_ULONG ( Table_Version ),
        FT_FRAME_ULONG ( Font_Revision ),
        FT_FRAME_LONG  ( CheckSum_Adjust ),
        FT_FRAME_LONG  ( Magic_Number ),
        FT_FRAME_USHORT( Flags ),
        FT_FRAME_USHORT( Units_Per_EM ),
        FT_FRAME_LONG  ( Created[0] ),
        FT_FRAME_LONG  ( Created[1] ),
        FT_FRAME_LONG  ( Modified[0] ),
        FT_FRAME_LONG  ( Modified[1] ),
        FT_FRAME_SHORT ( xMin ),
        FT_FRAME_SHORT ( yMin ),
        FT_FRAME_SHORT ( xMax ),
        FT_FRAME_SHORT ( yMax ),
        FT_FRAME_USHORT( Mac_Style ),
        FT_FRAME_USHORT( Lowest_Rec_PPEM ),
        FT_FRAME_SHORT ( Font_Direction ),
        FT_FRAME_SHORT ( Index_To_Loc_Format ),
        FT_FRAME_SHORT ( Glyph_Data_Format ),
      FT_FRAME_END
    };


    error = face->goto_table( face, tag, stream, 0 );
    if ( error )
      goto Exit;

    header = &face->header;

    if ( FT_STREAM_READ_FIELDS( header_fields, header ) )
      goto Exit;

    FT_TRACE3(( "Units per EM: %4u\n", header->Units_Per_EM ));
    FT_TRACE3(( "IndexToLoc:   %4d\n", header->Index_To_Loc_Format ));

  Exit:
    return error;
  }


  FT_LOCAL_DEF( FT_Error )
  tt_face_load_head( TT_Face    face,
                     FT_Stream  stream )
  {
    return tt_face_load_generic_header( face, stream, TTAG_head );
  }


#ifdef TT_CONFIG_OPTION_EMBEDDED_BITMAPS

  FT_LOCAL_DEF( FT_Error )
  tt_face_load_bhed( TT_Face    face,
                     FT_Stream  stream )
  {
    return tt_face_load_generic_header( face, stream, TTAG_bhed );
  }

#endif /* TT_CONFIG_OPTION_EMBEDDED_BITMAPS */


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_load_max_profile                                           */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Loads the maximum profile into a face object.                      */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face   :: A handle to the target face object.                      */
  /*                                                                       */
  /*    stream :: The input stream.                                        */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  FT_LOCAL_DEF( FT_Error )
  tt_face_load_maxp( TT_Face    face,
                     FT_Stream  stream )
  {
    FT_Error        error;
    TT_MaxProfile*  maxProfile = &face->max_profile;

    const FT_Frame_Field  maxp_fields[] =
    {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  TT_MaxProfile

      FT_FRAME_START( 6 ),
        FT_FRAME_LONG  ( version ),
        FT_FRAME_USHORT( numGlyphs ),
      FT_FRAME_END
    };

    const FT_Frame_Field  maxp_fields_extra[] =
    {
      FT_FRAME_START( 26 ),
        FT_FRAME_USHORT( maxPoints ),
        FT_FRAME_USHORT( maxContours ),
        FT_FRAME_USHORT( maxCompositePoints ),
        FT_FRAME_USHORT( maxCompositeContours ),
        FT_FRAME_USHORT( maxZones ),
        FT_FRAME_USHORT( maxTwilightPoints ),
        FT_FRAME_USHORT( maxStorage ),
        FT_FRAME_USHORT( maxFunctionDefs ),
        FT_FRAME_USHORT( maxInstructionDefs ),
        FT_FRAME_USHORT( maxStackElements ),
        FT_FRAME_USHORT( maxSizeOfInstructions ),
        FT_FRAME_USHORT( maxComponentElements ),
        FT_FRAME_USHORT( maxComponentDepth ),
      FT_FRAME_END
    };


    error = face->goto_table( face, TTAG_maxp, stream, 0 );
    if ( error )
      goto Exit;

    if ( FT_STREAM_READ_FIELDS( maxp_fields, maxProfile ) )
      goto Exit;

    maxProfile->maxPoints             = 0;
    maxProfile->maxContours           = 0;
    maxProfile->maxCompositePoints    = 0;
    maxProfile->maxCompositeContours  = 0;
    maxProfile->maxZones              = 0;
    maxProfile->maxTwilightPoints     = 0;
    maxProfile->maxStorage            = 0;
    maxProfile->maxFunctionDefs       = 0;
    maxProfile->maxInstructionDefs    = 0;
    maxProfile->maxStackElements      = 0;
    maxProfile->maxSizeOfInstructions = 0;
    maxProfile->maxComponentElements  = 0;
    maxProfile->maxComponentDepth     = 0;

    if ( maxProfile->version >= 0x10000L )
    {
      if ( FT_STREAM_READ_FIELDS( maxp_fields_extra, maxProfile ) )
        goto Exit;

      /* XXX: an adjustment that is necessary to load certain */
      /*      broken fonts like `Keystrokes MT' :-(           */
      /*                                                      */
      /*   We allocate 64 function entries by default when    */
      /*   the maxFunctionDefs field is null.                 */

      if ( maxProfile->maxFunctionDefs == 0 )
        maxProfile->maxFunctionDefs = 64;
    }

    FT_TRACE3(( "numGlyphs: %u\n", maxProfile->numGlyphs ));

  Exit:
    return error;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_load_names                                                 */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Loads the name records.                                            */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face   :: A handle to the target face object.                      */
  /*                                                                       */
  /*    stream :: The input stream.                                        */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  FT_LOCAL_DEF( FT_Error )
  tt_face_load_name( TT_Face    face,
                     FT_Stream  stream )
  {
    FT_Error      error;
    FT_Memory     memory = stream->memory;
    FT_ULong      table_pos, table_len;
    FT_ULong      storage_start, storage_limit;
    FT_UInt       count;
    TT_NameTable  table;

    static const FT_Frame_Field  name_table_fields[] =
    {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  TT_NameTableRec

      FT_FRAME_START( 6 ),
        FT_FRAME_USHORT( format ),
        FT_FRAME_USHORT( numNameRecords ),
        FT_FRAME_USHORT( storageOffset ),
      FT_FRAME_END
    };

    static const FT_Frame_Field  name_record_fields[] =
    {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  TT_NameEntryRec

      /* no FT_FRAME_START */
        FT_FRAME_USHORT( platformID ),
        FT_FRAME_USHORT( encodingID ),
        FT_FRAME_USHORT( languageID ),
        FT_FRAME_USHORT( nameID ),
        FT_FRAME_USHORT( stringLength ),
        FT_FRAME_USHORT( stringOffset ),
      FT_FRAME_END
    };


    table         = &face->name_table;
    table->stream = stream;

    error = face->goto_table( face, TTAG_name, stream, &table_len );
    if ( error )
      goto Exit;

    table_pos = FT_STREAM_POS();


    if ( FT_STREAM_READ_FIELDS( name_table_fields, table ) )
      goto Exit;

    /* Some popular Asian fonts have an invalid `storageOffset' value   */
    /* (it should be at least "6 + 12*num_names").  However, the string */
    /* offsets, computed as "storageOffset + entry->stringOffset", are  */
    /* valid pointers within the name table...                          */
    /*                                                                  */
    /* We thus can't check `storageOffset' right now.                   */
    /*                                                                  */
    storage_start = table_pos + 6 + 12*table->numNameRecords;
    storage_limit = table_pos + table_len;

    if ( storage_start > storage_limit )
    {
      FT_ERROR(( "invalid `name' table\n" ));
      error = SFNT_Err_Name_Table_Missing;
      goto Exit;
    }

    /* Allocate the array of name records. */
    count                 = table->numNameRecords;
    table->numNameRecords = 0;

    if ( FT_NEW_ARRAY( table->names, count ) ||
         FT_FRAME_ENTER( count * 12 )        )
      goto Exit;

    /* Load the name records and determine how much storage is needed */
    /* to hold the strings themselves.                                */
    {
      TT_NameEntryRec*  entry = table->names;


      for ( ; count > 0; count-- )
      {
        if ( FT_STREAM_READ_FIELDS( name_record_fields, entry ) )
          continue;

        /* check that the name is not empty */
        if ( entry->stringLength == 0 )
          continue;

        /* check that the name string is within the table */
        entry->stringOffset += table_pos + table->storageOffset;
        if ( entry->stringOffset                       < storage_start ||
             entry->stringOffset + entry->stringLength > storage_limit )
        {
          /* invalid entry - ignore it */
          entry->stringOffset = 0;
          entry->stringLength = 0;
          continue;
        }

        entry++;
      }

      table->numNameRecords = (FT_UInt)( entry - table->names );
    }

    FT_FRAME_EXIT();

    /* everything went well, update face->num_names */
    face->num_names = (FT_UShort) table->numNameRecords;

  Exit:
    return error;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_free_names                                                 */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Frees the name records.                                            */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face :: A handle to the target face object.                        */
  /*                                                                       */
  FT_LOCAL_DEF( void )
  tt_face_free_name( TT_Face  face )
  {
    FT_Memory     memory = face->root.driver->root.memory;
    TT_NameTable  table  = &face->name_table;
    TT_NameEntry  entry  = table->names;
    FT_UInt       count  = table->numNameRecords;


    if ( table->names )
    {
      for ( ; count > 0; count--, entry++ )
      {
        FT_FREE( entry->string );
        entry->stringLength = 0;
      }

      /* free strings table */
      FT_FREE( table->names );
    }

    table->numNameRecords = 0;
    table->format         = 0;
    table->storageOffset  = 0;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_load_cmap                                                  */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Loads the cmap directory in a face object.  The cmaps themselves   */
  /*    are loaded on demand in the `ttcmap.c' module.                     */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face   :: A handle to the target face object.                      */
  /*                                                                       */
  /*    stream :: A handle to the input stream.                            */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */

  FT_LOCAL_DEF( FT_Error )
  tt_face_load_cmap( TT_Face    face,
                     FT_Stream  stream )
  {
    FT_Error  error;


    error = face->goto_table( face, TTAG_cmap, stream, &face->cmap_size );
    if ( error )
      goto Exit;

    if ( FT_FRAME_EXTRACT( face->cmap_size, face->cmap_table ) )
      face->cmap_size = 0;

  Exit:
    return error;
  }



  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_load_os2                                                   */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Loads the OS2 table.                                               */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face   :: A handle to the target face object.                      */
  /*                                                                       */
  /*    stream :: A handle to the input stream.                            */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  FT_LOCAL_DEF( FT_Error )
  tt_face_load_os2( TT_Face    face,
                    FT_Stream  stream )
  {
    FT_Error  error;
    TT_OS2*   os2;

    const FT_Frame_Field  os2_fields[] =
    {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  TT_OS2

      FT_FRAME_START( 78 ),
        FT_FRAME_USHORT( version ),
        FT_FRAME_SHORT ( xAvgCharWidth ),
        FT_FRAME_USHORT( usWeightClass ),
        FT_FRAME_USHORT( usWidthClass ),
        FT_FRAME_SHORT ( fsType ),
        FT_FRAME_SHORT ( ySubscriptXSize ),
        FT_FRAME_SHORT ( ySubscriptYSize ),
        FT_FRAME_SHORT ( ySubscriptXOffset ),
        FT_FRAME_SHORT ( ySubscriptYOffset ),
        FT_FRAME_SHORT ( ySuperscriptXSize ),
        FT_FRAME_SHORT ( ySuperscriptYSize ),
        FT_FRAME_SHORT ( ySuperscriptXOffset ),
        FT_FRAME_SHORT ( ySuperscriptYOffset ),
        FT_FRAME_SHORT ( yStrikeoutSize ),
        FT_FRAME_SHORT ( yStrikeoutPosition ),
        FT_FRAME_SHORT ( sFamilyClass ),
        FT_FRAME_BYTE  ( panose[0] ),
        FT_FRAME_BYTE  ( panose[1] ),
        FT_FRAME_BYTE  ( panose[2] ),
        FT_FRAME_BYTE  ( panose[3] ),
        FT_FRAME_BYTE  ( panose[4] ),
        FT_FRAME_BYTE  ( panose[5] ),
        FT_FRAME_BYTE  ( panose[6] ),
        FT_FRAME_BYTE  ( panose[7] ),
        FT_FRAME_BYTE  ( panose[8] ),
        FT_FRAME_BYTE  ( panose[9] ),
        FT_FRAME_ULONG ( ulUnicodeRange1 ),
        FT_FRAME_ULONG ( ulUnicodeRange2 ),
        FT_FRAME_ULONG ( ulUnicodeRange3 ),
        FT_FRAME_ULONG ( ulUnicodeRange4 ),
        FT_FRAME_BYTE  ( achVendID[0] ),
        FT_FRAME_BYTE  ( achVendID[1] ),
        FT_FRAME_BYTE  ( achVendID[2] ),
        FT_FRAME_BYTE  ( achVendID[3] ),

        FT_FRAME_USHORT( fsSelection ),
        FT_FRAME_USHORT( usFirstCharIndex ),
        FT_FRAME_USHORT( usLastCharIndex ),
        FT_FRAME_SHORT ( sTypoAscender ),
        FT_FRAME_SHORT ( sTypoDescender ),
        FT_FRAME_SHORT ( sTypoLineGap ),
        FT_FRAME_USHORT( usWinAscent ),
        FT_FRAME_USHORT( usWinDescent ),
      FT_FRAME_END
    };

    const FT_Frame_Field  os2_fields_extra[] =
    {
      FT_FRAME_START( 8 ),
        FT_FRAME_ULONG( ulCodePageRange1 ),
        FT_FRAME_ULONG( ulCodePageRange2 ),
      FT_FRAME_END
    };

    const FT_Frame_Field  os2_fields_extra2[] =
    {
      FT_FRAME_START( 10 ),
        FT_FRAME_SHORT ( sxHeight ),
        FT_FRAME_SHORT ( sCapHeight ),
        FT_FRAME_USHORT( usDefaultChar ),
        FT_FRAME_USHORT( usBreakChar ),
        FT_FRAME_USHORT( usMaxContext ),
      FT_FRAME_END
    };


    /* We now support old Mac fonts where the OS/2 table doesn't  */
    /* exist.  Simply put, we set the `version' field to 0xFFFF   */
    /* and test this value each time we need to access the table. */
    error = face->goto_table( face, TTAG_OS2, stream, 0 );
    if ( error )
      goto Exit;

    os2 = &face->os2;

    if ( FT_STREAM_READ_FIELDS( os2_fields, os2 ) )
      goto Exit;

    os2->ulCodePageRange1 = 0;
    os2->ulCodePageRange2 = 0;
    os2->sxHeight         = 0;
    os2->sCapHeight       = 0;
    os2->usDefaultChar    = 0;
    os2->usBreakChar      = 0;
    os2->usMaxContext     = 0;

    if ( os2->version >= 0x0001 )
    {
      /* only version 1 tables */
      if ( FT_STREAM_READ_FIELDS( os2_fields_extra, os2 ) )
        goto Exit;

      if ( os2->version >= 0x0002 )
      {
        /* only version 2 tables */
        if ( FT_STREAM_READ_FIELDS( os2_fields_extra2, os2 ) )
          goto Exit;
      }
    }

    FT_TRACE3(( "sTypoAscender:  %4d\n",   os2->sTypoAscender ));
    FT_TRACE3(( "sTypoDescender: %4d\n",   os2->sTypoDescender ));
    FT_TRACE3(( "usWinAscent:    %4u\n",   os2->usWinAscent ));
    FT_TRACE3(( "usWinDescent:   %4u\n",   os2->usWinDescent ));
    FT_TRACE3(( "fsSelection:    0x%2x\n", os2->fsSelection ));

  Exit:
    return error;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_load_postscript                                            */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Loads the Postscript table.                                        */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face   :: A handle to the target face object.                      */
  /*                                                                       */
  /*    stream :: A handle to the input stream.                            */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  FT_LOCAL_DEF( FT_Error )
  tt_face_load_post( TT_Face    face,
                     FT_Stream  stream )
  {
    FT_Error        error;
    TT_Postscript*  post = &face->postscript;

    static const FT_Frame_Field  post_fields[] =
    {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  TT_Postscript

      FT_FRAME_START( 32 ),
        FT_FRAME_ULONG( FormatType ),
        FT_FRAME_ULONG( italicAngle ),
        FT_FRAME_SHORT( underlinePosition ),
        FT_FRAME_SHORT( underlineThickness ),
        FT_FRAME_ULONG( isFixedPitch ),
        FT_FRAME_ULONG( minMemType42 ),
        FT_FRAME_ULONG( maxMemType42 ),
        FT_FRAME_ULONG( minMemType1 ),
        FT_FRAME_ULONG( maxMemType1 ),
      FT_FRAME_END
    };


    error = face->goto_table( face, TTAG_post, stream, 0 );
    if ( error )
      return error;

    if ( FT_STREAM_READ_FIELDS( post_fields, post ) )
      return error;

    /* we don't load the glyph names, we do that in another */
    /* module (ttpost).                                     */

    FT_TRACE3(( "FormatType:   0x%x\n", post->FormatType ));
    FT_TRACE3(( "isFixedPitch:   %s\n", post->isFixedPitch
                                        ? "  yes" : "   no" ));

    return SFNT_Err_Ok;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_load_pclt                                                  */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Loads the PCL 5 Table.                                             */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face   :: A handle to the target face object.                      */
  /*                                                                       */
  /*    stream :: A handle to the input stream.                            */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  FT_LOCAL_DEF( FT_Error )
  tt_face_load_pclt( TT_Face    face,
                     FT_Stream  stream )
  {
    static const FT_Frame_Field  pclt_fields[] =
    {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  TT_PCLT

      FT_FRAME_START( 54 ),
        FT_FRAME_ULONG ( Version ),
        FT_FRAME_ULONG ( FontNumber ),
        FT_FRAME_USHORT( Pitch ),
        FT_FRAME_USHORT( xHeight ),
        FT_FRAME_USHORT( Style ),
        FT_FRAME_USHORT( TypeFamily ),
        FT_FRAME_USHORT( CapHeight ),
        FT_FRAME_BYTES ( TypeFace, 16 ),
        FT_FRAME_BYTES ( CharacterComplement, 8 ),
        FT_FRAME_BYTES ( FileName, 6 ),
        FT_FRAME_CHAR  ( StrokeWeight ),
        FT_FRAME_CHAR  ( WidthType ),
        FT_FRAME_BYTE  ( SerifStyle ),
        FT_FRAME_BYTE  ( Reserved ),
      FT_FRAME_END
    };

    FT_Error  error;
    TT_PCLT*  pclt = &face->pclt;


    /* optional table */
    error = face->goto_table( face, TTAG_PCLT, stream, 0 );
    if ( error )
      goto Exit;

    if ( FT_STREAM_READ_FIELDS( pclt_fields, pclt ) )
      goto Exit;

  Exit:
    return error;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    tt_face_load_gasp                                                  */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Loads the `gasp' table into a face object.                         */
  /*                                                                       */
  /* <Input>                                                               */
  /*    face   :: A handle to the target face object.                      */
  /*                                                                       */
  /*    stream :: The input stream.                                        */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  FT_LOCAL_DEF( FT_Error )
  tt_face_load_gasp( TT_Face    face,
                     FT_Stream  stream )
  {
    FT_Error   error;
    FT_Memory  memory = stream->memory;

    FT_UInt        j,num_ranges;
    TT_GaspRange   gaspranges;


    /* the gasp table is optional */
    error = face->goto_table( face, TTAG_gasp, stream, 0 );
    if ( error )
      goto Exit;

    if ( FT_FRAME_ENTER( 4L ) )
      goto Exit;

    face->gasp.version   = FT_GET_USHORT();
    face->gasp.numRanges = FT_GET_USHORT();

    FT_FRAME_EXIT();

    /* only support versions 0 and 1 of the table */
    if ( face->gasp.version >= 2 )
    {
      face->gasp.numRanges = 0;
      error = SFNT_Err_Invalid_Table;
      goto Exit;
    }

    num_ranges = face->gasp.numRanges;
    FT_TRACE3(( "numRanges: %u\n", num_ranges ));

    if ( FT_QNEW_ARRAY( gaspranges, num_ranges ) ||
         FT_FRAME_ENTER( num_ranges * 4L )      )
      goto Exit;

    face->gasp.gaspRanges = gaspranges;

    for ( j = 0; j < num_ranges; j++ )
    {
      gaspranges[j].maxPPEM  = FT_GET_USHORT();
      gaspranges[j].gaspFlag = FT_GET_USHORT();

      FT_TRACE3(( "gaspRange %d: rangeMaxPPEM %5d, rangeGaspBehavior 0x%x\n",
                  j,
                  gaspranges[j].maxPPEM,
                  gaspranges[j].gaspFlag ));
    }

    FT_FRAME_EXIT();

  Exit:
    return error;
  }


/* END */
