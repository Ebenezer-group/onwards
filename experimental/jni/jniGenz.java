public class jniGenz{
  private native void wrap (String[] av);

  public static void main (String[] args){
      System.loadLibrary("native");
      new jniGenz().wrap(args);
  }
}
