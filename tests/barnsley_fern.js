/* Barnsley fern display on a color terminal
   (c) 2026 Antonin Gavrel
   MIT license
*/
var W=80,H=32,N=3e4,G,S,x,y,Y,Q=0,i,r,a,b,p,q,z,o,v,c;
var CH=" .:-=+*#%@";
var M=[0,0,0,.16,0,.85,.04,-.04,.85,1.6,.2,-.26,.23,.22,1.6,-.15,.28,.26,.24,.44];
function R(){return(S=(S*1664525+1013904223)%4294967296)/4294967296}
for(Y=0;Y<H;Y++){
    G=new Uint16Array(W);
    S=1;x=0;y=0;
    for(i=N;i--;){
        r=R()*100|0;
        r=5*(3-(r-1>>>31)-(r-86>>>31)-(r-93>>>31));
        a=M[r]*x+M[r+1]*y;
        b=M[r+2]*x+M[r+3]*y+M[r+4];
        x=a;y=b;
        p=Math.floor((x+2.182)*(W-1)/4.8378);
        q=H-1-Math.floor(y*(H-1)/9.9983);
        if(p>=0&&p<W&&q==Y)G[p]++;
    }
    for(o="",c=0,x=0;x<W;x++) {
        z=Y*W+x;
        v=Math.floor(Math.log(G[x]+1)*1.62);
        if(v&&v!=c)o+="\x1b[38;5;"+(16+6*v+30*(v-5&~(v-5>>31)))+"m",c=v;
        o+=CH[v];
        Q=(Q+(z+1)*G[x])%2147483647;
    }
    console.log(o+"\x1b[0m");
}
if(Q!=31266866)throw Error("invalid Barnsley fern checksum")
