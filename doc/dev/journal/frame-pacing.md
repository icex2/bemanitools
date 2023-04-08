# frame pacing

## 10th style

```c
// sub_4790D0
int __thiscall frame_pace_outer(_DWORD *this, int a2)
{
  DWORD v3; // ecx
  int v4; // eax
  int v5; // ecx

  v3 = 625 * (timeGetTime() - this[63]);
  v4 = this[61];
  v5 = 16 * v3;
  if ( v5 < 2 * v4 || v5 < 2 * this[62] )
    this[61] = (v5 + 2 * v4 + v4) / 4;
  this[62] = v5;
  return frame_pace_maybe(this);
}
```

```c
// sub_479030
void __thiscall frame_pace_maybe(_DWORD *this)
{
  int v1; // ecx

  v1 = this[60];
  if ( v1 <= 0 )
    Sleep(0);
  else
    Sleep(v1 / 10000);
}
```