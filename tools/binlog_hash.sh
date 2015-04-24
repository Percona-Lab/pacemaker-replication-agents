# get the binlog references and md5 of the payload.  Currently unable to report
# the 1st trx.
get_last_binlog() {
   local binlog_files
   local maxEntries
   local startAt nlines
   local bltempfile
   
   queryok=0
   bltempfile=`mktemp ${HA_RSCTMP}/${OCF_RESOURCE_INSTANCE}.XXXXXX`
   ( for blf in $1; do 
      blf_no_path=`echo "$blf" | rev | cut -d'/' -f1 | rev`
      mysqlbinlog --base64-output=DECODE-ROWS -vvv  $blf | \
      ( while read line; do
         if [ "$queryok" -eq 1 ]; then
            if [[ $line =~ ^\# ]]; then
               if [[ $line =~ .*end_log_pos\ ([0-9]*).*Xid\ =\ [0-9]*$ ]]; then
                 echo "${BASH_REMATCH[1]},`md5sum $bltempfile | cut -d' ' -f1`"
                 echo -n '' > $bltempfile
               else
                  if [[ $line =~ ^\#\#\# ]]; then
                     echo "${line}" >> $bltempfile
                  fi
               fi
            else
               echo "${line}" >> $bltempfile
            fi
         else
            if [[ $line =~ ^BEGIN ]]; then
               queryok=1
            fi
         fi
      done ) 
   done ) 
   rm -f $bltempfile
}

get_last_binlog $1 
