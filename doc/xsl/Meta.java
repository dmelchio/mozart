import java.util.Hashtable;

public class Meta {
    private static Hashtable table_map = new Hashtable();
    public static boolean latexTableSpecPut(String id,String spec) {
	table_map.put(id.toUpperCase(),spec);
	return true;
    }
    public static String latexTableSpecGet(String id) {
	return (String) table_map.get(id);
    }
    public static boolean latexTableSpecExists(String id) {
	return table_map.containsKey(id);
    }

    private static Hashtable mode_map = new Hashtable();
    public static boolean emacsModePut(String mode,String elisp) {
	mode_map.put(mode.toUpperCase(),elisp);
	return true;
    }
    public static String emacsModeGet(String mode) {
	if (mode_map.containsKey(mode))
	    return (String) mode_map.get(mode);
	else
	    return mode.toLowerCase();
    }
    private static Hashtable picwid_map = new Hashtable();
    public static boolean pictureWidthPut(String id,String wd) {
	picwid_map.put(id.toUpperCase(),wd);
	return true;
    }
    public static boolean pictureWidthExists(String id) {
	return picwid_map.containsKey(id.toUpperCase());
    }
    public static String pictureWidthGet(String id) {
	return (String) picwid_map.get(id.toUpperCase());
    }
}
