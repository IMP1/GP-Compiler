module OILR3.CBackend (hostToC, progToC) where

import OILR3.Instructions
import OILR3.CRuntime
import OILR3.IR

import Mapping

import Data.List
import Data.Bits
import Debug.Trace


data C = Block [C]
       | Call id [C]

data CDefn = CDefn id C

{- data Instr =
      OILR Int          -- Number of OILR indices
    -- Graph modification
    | ABN Dst           -- Add and Bind Node to register Dst
    | ABE Dst Src Tgt   -- Add and Bind Edge to register Dst between nodes in Src & Tgt
    | DBN Dst           -- Delete Bound Node 
    | DBE Dst           -- Delete Bound Node
    
    | RBN Dst Bool      -- set Root on Bound Node to Bool
    
    | CBL Dst Col       -- Colour Bound eLement
    | LBL Dst Int       -- Label Bound eLement with Int

    -- Graph search
    | BND Dst Spc          -- Bind next unbound NoDe in Spc to Dst
    | BED Dst Reg Reg Dir  -- Bind EDge between Regs in Dir
    | BEN Dst Dst Src Dir  -- Bind Edge and Node by following an edge in Dir from Src
    | NEC Src Tgt          -- Negative Edge Condition from Src to Tgt

    -- Definitions & program structure
    | DEF Id               -- DEFine function Idopen source dev site
    | CAL Id               -- CALl Id, pushing current IP to call-stack
    | TAR Target           -- jump TARget
    | BRZ Target           -- BRanch if Zero (i.e. if bool flag is false)
    | BNZ Target           -- Branch if Non-Zero
    | BRA Target           -- Branch RAndomly. Take the branch 50% of the time.
    | BRN Target           -- unconditional BRaNch to Target
    | RET                  -- RETurn to IP on top of call-stack
    | RTZ                  -- ReTurn if Zero
    | RNZ                  -- Return if Non-Zero

    -- Backtracking
    | BBT                  -- Begin BackTracking section
    | BAK                  -- unconditionally roll-BAcK backtracking section changes
    | EBT                  -- End BackTracking secion: commit if flag is true, rollback otherwise
    -- There is no rollback command. This needs to be done manually with reverse rules.

    -- Stack machine
    | BLO Dst              -- push Bound eLement Out-degree to stack
    | BLI Dst              -- push Bound eLement In-degree to stack
    | BLL Dst              -- push Bound eLement looP-degree to stack
    | BLR Dst              -- push Bound eLement Rootedness to stack
    | BLN Dst              -- push Bound eLement's Numeric label to stack
    | BLC Dst              -- push Bound eLement Colour to stack

    | SHL Int              -- SHift top-of-stack Left by Int bits
    | OR                   -- bitwise OR top 2 values on the stack
    | AND                  -- bitwise AND top 2 value on the stack

    -- Misc
    | NOP                  -- No-OP
    | TRU                  -- set the boolean register to TRUe
    | FLS                  -- set the boolean register to FaLSe
    deriving (Show, Eq) -}

hostToC :: OilrRule -> CDefn
hostToC 

hostCompileInstruction :: OilrIR -> String
hostCompileInstruction 




{-
type OilrProg = [Instr Int Int]
data OilrIndexBits = OilrIndexBits { cBits::Int
                                   , oBits::Int
                                   , iBits::Int
                                   , lBits::Int
                                   , rBits::Int } deriving (Show, Eq)

data OilrProps = OilrProps { flags   :: [Flag]
                           , spaces  :: Mapping Pred [Int]
                           , oilrInd :: OilrIndexBits
                           , stateMask :: Mapping String [Int] 
                           , recMask :: String  -- recursion limit mask
                           , nSlots   :: Mapping String Int }

hostToC :: OilrProg -> String
hostToC is = makeCFunction "_HOST" $ map hostCompileInstruction is

hostCompileInstruction :: Instr Int Int -> String
hostCompileInstruction (ADN n)   = makeCFunctionCall "addNode" []
hostCompileInstruction (ADE e src tgt) = makeCFunctionCallIntArgs "addEdgeById" [src,tgt]
hostCompileInstruction (RTN n)   = makeCFunctionCallIntArgs "setRootById" [n]
hostCompileInstruction (CON n c) = makeCFunctionCallIntArgs "setColourById" [n, c]
hostCompileInstruction NOP       = ""
hostCompileInstruction i         = error $ "Instruction " ++ show i ++ " not implemented for host graphs"

progToC :: [Flag] -> [OilrProg] -> String
progToC flags decls = consts ++ cRuntime ++ cCode
    where
        oilr = analyseProg flags decls
        cCode = intercalate "\n" $ prototypes decls
                                ++ concatMap (compileDefn oilr) decls
        ind = oilrInd oilr
        consts = concat [ "#define OILR_C_BITS (" ++ (show $ cBits ind) ++ ")\n"
                        , "#define OILR_O_BITS (" ++ (show $ oBits ind) ++ ")\n"
                        , "#define OILR_I_BITS (" ++ (show $ iBits ind) ++ ")\n"
                        , "#define OILR_L_BITS (" ++ (show $ lBits ind) ++ ")\n"
                        , "#define OILR_R_BITS (" ++ (show $ rBits ind) ++ ")\n"
                        , case ( EnableDebugging         `elem` flags
                               , EnableParanoidDebugging `elem` flags) of 
                            (False, False) -> "#define NDEBUG\n"
                            (_, True)      -> "#define OILR_PARANOID_CHECKS\n"
                            _              -> ""
                        , if CompactLists `elem` flags
                            then "#define OILR_COMPACT_LISTS\n"
                            else ""
                        , if EnableExecutionTrace `elem` flags
                            then "#define OILR_EXECUTION_TRACE\n"
                            else ""  ]

analyseProg :: [Flag] -> [OilrProg] -> OilrProps
analyseProg fs decls = OilrProps { flags = fs , spaces = spcs , oilrInd = bits
                                 , nSlots = slots, recMask = rMask, stateMask = sMask }
    where
        spcs = makeSearchSpaces capBits bits $ concat decls
        bits = oilrBits capBits decls
        capBits = if DisableOilr `elem` fs then 1 else 16
        slots = map makeSlots decls
        sMask  = map makeMask decls
        rMask  = if Compile32Bit `elem` fs then "0xfff" else "0x1fff"

makeSlots :: OilrProg -> (String, Int)
makeSlots is = ( idFor is , sum $ map countMatches is )
    where
        countMatches (LUN _ _)     = 1
        countMatches (LUE _ _ _ _) = 1
        countMatches (XTE _ _ _ _) = 2
        countMatches (ADN _)       = 1
        countMatches (ADE _ _ _)   = 1
        countMatches _             = 0

makeMask :: OilrProg -> (String, [Int])
makeMask is = ( idFor is , concatMap mask is )
    where
        mask (LUN _ _)     = [-1]
        mask (LUE _ _ _ _) = [0]
        mask (XTE _ _ _ _) = [0, 0]
        mask _             = []

idFor :: OilrProg -> String
idFor (PRO s:_) = s
idFor (RUL s:_) = s
idFor is = error $ "Malformed rule or procedure body: " ++ show is

slotsFor :: OilrProps -> String -> String
slotsFor oilr id = intercalate "," $ take n $ repeat "NULL"
    where
        n = definiteLookup id $ nSlots oilr

maskFor :: OilrProps -> String -> String
maskFor oilr id = intercalate "," $ map show $ definiteLookup id $ stateMask oilr

-- Generate C prototypes so that the ordering of definitions
-- doesn't matter
prototypes :: [OilrProg] -> [String]
prototypes decls = map proto decls
    where
        proto ((PRO "Main"):_) = ""
        proto ((PRO s):_) = "\nvoid " ++ s ++ "();"
        proto ((RUL s):_) = "\nvoid " ++ s ++ "(long recursive, DList **state);"
        proto _ = error "Found an ill-formed definition"


oilrIndexTotalBits :: OilrIndexBits -> Int
oilrIndexTotalBits (OilrIndexBits c o i l r) = c+o+i+l+r

extractPredicates :: OilrProg -> [Pred]
extractPredicates is = concatMap harvestPred is
    where harvestPred (LUN _ p) = [p]
          harvestPred _         = []

oilrBits :: Int -> [OilrProg] -> OilrIndexBits
oilrBits cap iss = OilrIndexBits (fc c) (f o) (f i) (f l) (f' r)
    -- We can't cap the r dimension, because there's no other check for the root flag.
    where
        fc x = if f' x == 0 then 0 else ( bits $ length colourIds )
        f x  = min cap $ f' x
        f'   = (bits . maximum . map extract)
        (c, o, i, l, r) = unzip5 $ explodePreds $ extractPredicates $ concat iss
        explodePreds :: [Pred] -> [(Dim, Dim, Dim, Dim, Dim)]
        explodePreds prs = [ (cDim pr, oDim pr, iDim pr, lDim pr, rDim pr) | pr <- prs ]
        extract (Equ n) = n
        extract (GtE n) = n
        bits n = head $ dropWhile (\x -> 2^x <= n) [0,1..]

sigsForPred :: Int -> OilrIndexBits -> Pred -> (Pred, [Int])
sigsForPred cap bits pr = (pr, nub [   c' `shift` cShift + o' `shift` oShift
                                     + i' `shift` iShift + l' `shift` lShift
                                     + r' `shift` rShift
                                   | c' <- sigForDim (cBits bits) c
                                   , o' <- sigForDim (oBits bits) o
                                   , i' <- sigForDim (iBits bits) i
                                   , l' <- sigForDim (lBits bits) l
                                   , r' <- sigForDim (rBits bits) r ])
    where
        ( c, o, i, l, r ) = (cDim pr, oDim pr, iDim pr, lDim pr, rDim pr)
        capSize = (1 `shift` cap) - 1
        cShift = oShift + (min cap $ oBits bits)
        oShift = iShift + (min cap $ iBits bits)
        iShift = lShift + (min cap $ lBits bits)
        lShift = rShift + rBits bits
        rShift = 0

        sigForDim :: Int -> Dim -> [Int]
        sigForDim _ (Equ n) = [ min capSize n ]
        sigForDim b (GtE n) = [ min capSize n .. ( 1 `shift` b)-1 ]


makeSearchSpaces :: Int -> OilrIndexBits -> OilrProg -> Mapping Pred [Int]
makeSearchSpaces cap bits is = map (sigsForPred cap bits) $ trace (show preds) preds
    where
        preds = ( nub . extractPredicates ) is


compileDefn :: OilrProps -> OilrProg -> [String]
compileDefn oilr is@(PRO _:_) = compileProc oilr is 
compileDefn oilr is@(RUL _:_) = compileRule oilr is
compileDefn _ (i:is)  = error $ "Found a definition starting with " ++ show i


compileProc :: OilrProps -> OilrProg -> [String]
compileProc oilr is = map compile is
    where
        compile (PRO "Main") = compile (PRO "_GPMAIN")
        compile (PRO id) = intercalate "\n" [ redef "DONE" "return"
                                            , redef "RECURSE" "" 
                                            , startCFunction id [] ]
        compile (CAL id) = makeCFunctionCall "CALL" [id, slotsFor oilr id]
        compile (ALP id) =
            makeCFunctionCall "ALAP" [id, useRecursion oilr, slotsFor oilr id]
        compile END      = endCFunction
        compile i  =  error $ "Unexpected instruction: " ++ show i

redef :: String -> String -> String
redef macro code = intercalate "\n" [ "#undef " ++  macro
                                    , concat [ "#define ", macro, " ", code ] ]

unbindAndRet :: OilrProps -> String -> String
unbindAndRet oilr id =
    concat [ "do {"
           , makeCFunctionCall "trace" ["boolFlag?'s':'f'"]
           , makeCFunctionCall "unbindAll" ["matches", show $ definiteLookup id $ nSlots oilr]
           , "return;"
           , "} while (0);" ]

-- The mask on "recursive" is to prevent stack-overflows from too much
-- recursion TODO: the limit could be tuned to the individual rule, based on
-- the number and type of travs in the rule (as the largest component of the
-- stack frame is the matches[] array.
recursionCode :: String -> String -> String
recursionCode rMask id = makeCFunctionCall id [ "(recursive+1)&" ++ rMask, "state" ]

useRecursion :: OilrProps -> String
useRecursion oilr = if RecursiveRules `elem` flags oilr then "1" else "0"

matchComplete :: OilrProps -> String
matchComplete oilr = if useRecursion oilr == "1"
                        then "if (recursive) { RECURSE; boolFlag=1; }\n"
                        else ""

compileRule :: OilrProps -> OilrProg -> [String]
compileRule oilr is = map compile is
    where
        compile (RUL id)    = intercalate "\n" $ ruleHeader oilr id
        compile ORF         = orFail
        compile (ORB n)     = orBack n
        compile (ADN n)     = addBoundNode n
        compile (ADE e s t) = addBoundEdge e s t
        compile (RTN n)     = setBoundRoot n
        compile (URN n)     = unsetBoundRoot n
        compile (CON n c)   = setBoundColour n c
        compile (DEN n)     = deleteBoundNode n
        compile (DEE e)     = deleteBoundEdge e
        compile UBA         = exitRule
        compile OK          = matchComplete oilr
        compile RET         = "return;"
        compile END         = endCFunction
        compile i           = compileTrav oilr i

compileTrav :: OilrProps -> Instr Int Int -> String
compileTrav oilr i = intercalate "\n" [ travHeader oilr i, travBody oilr i ]

travHeader :: OilrProps -> Instr Int Int -> String
travHeader oilr i = 
    intercalate "\n" [ labelFor (idOfInstr i) ++ ":"
                     , makeCFunctionCall "debug" [ show "In trav %s\n", show $ show i ]
                     , makeCFunctionCall "checkGraph" [] ]
    where idOfInstr (LUN n _)     = n
          idOfInstr (LUE e _ _ _) = e
          idOfInstr (XTE _ e _ _) = e

travBody :: OilrProps -> Instr Int Int -> String
travBody oilr (LUN n sig) = case definiteLookup sig $ spaces oilr of
    [s] -> makeCFunctionCall "makeSimpleTrav" [ show n , "index(" ++ show s ++ ")"]
    ss  -> makeCFunctionCall "makeTrav" ( show n
                                        : show (length ss)
                                        : ["{0, index("++show s++")}" | s <- ss] )
travBody _ i@(LUE e s t Either) = makeCFunctionCallIntArgs "makeBidiEdgeTrav" [s, e, t]
travBody _ i@(LUE e s t Out)
    | s == t    = makeCFunctionCallIntArgs "makeLoopTrav" [s, e]
    | otherwise = makeCFunctionCallIntArgs "makeEdgeTrav" [s, e, t]
travBody _ i@(XTE s e t In)  = makeCFunctionCallIntArgs "makeExtendInTrav"  [s, e, t, 1]
travBody _ i@(XTE s e t Out) = makeCFunctionCallIntArgs "makeExtendOutTrav" [s, e, t, 1]
travBody _ (NEC s t) = makeCFunctionCallIntArgs "makeAntiEdgeTrav" [s, t]

ruleHeader :: OilrProps -> String -> [String]
ruleHeader oilr id = [ redef "DONE" $ unbindAndRet oilr id
                     , redef "RECURSE" $ recursionCode (recMask oilr) id
                     , startCFunction id [("long", "recursive")
                                         ,("DList", "**state")]
                     , concat ["\tElement *matches[] = { NULL, " , slots, "};"]
                     , intercalate "\n" [ "#ifdef OILR_EXECUTION_TRACE"
                                        , "\toilrCurrentRule=" ++ show id ++ ";"
                                        , "#endif" ]
                     -- making the below const makes no difference to
                     -- the code generated by gcc 4.8.4 with -O2. drat
                     , concat [ "\tlong stateMask[] = { ", mask, "};"]
                     , concat [ "\tlong i; for (i=0; i<"
                              , show maskSize
                              , "; i++) state[i] = (DList*) ((long) state[i] & stateMask[i]);" ]
                     , "" ]
    where
        slots = slotsFor oilr id
        mask  = maskFor  oilr id
        maskSize = length $ definiteLookup id $ stateMask oilr

orFail :: String
orFail = concat [ "\tif (!boolFlag) " , exitRule , ";"]

orBack :: Int -> String
orBack n = concat [ "\tif (!boolFlag) goto " , labelFor n , ";"]

addBoundNode :: Int -> String
addBoundNode n = modifyAndBind n "addNode" []

addBoundEdge :: Int -> Int -> Int -> String
addBoundEdge e s t = if s==t
                        then modifyAndBind e "addLoop" $ [ bindingFor s ]
                        else modifyAndBind e "addEdge" $ map bindingFor [s, t]

deleteBoundNode :: Int -> String
deleteBoundNode n = makeCFunctionCall "deleteNode" [ bindingFor n ]

deleteBoundEdge :: Int -> String
deleteBoundEdge e = makeCFunctionCall "deleteEdge" [ bindingFor e ]

setBoundRoot :: Int -> String
setBoundRoot n = makeCFunctionCall "setRoot" [ asNode $ bindingFor n ]

unsetBoundRoot :: Int -> String
unsetBoundRoot n = makeCFunctionCall "unsetRoot" [ asNode $ bindingFor n ]

setBoundColour :: Int -> Int -> String
setBoundColour n c = makeCFunctionCall "setColour" [ asNode (bindingFor n) , show c]

asNode :: String -> String
asNode s = concat [ "asNode(", s, ")" ]

bindingFor :: Int -> String
bindingFor n = concat [ "matches[" , show n , "]" ]

exitRule :: String
exitRule = "\tDONE;"

modifyAndBind :: Int -> String -> [String] -> String
modifyAndBind i fun args = concat [ '\t' : bindingFor i , " = ", makeCFunctionCall fun args ]

makeModifyAndBind :: Int -> String -> [Int] -> String
makeModifyAndBind i fun args = "\t" ++ matchInd i ++ " = " ++ makeCFunctionCall fun (map matchInd args)
    where
        matchInd i = "matches[" ++ show i ++ "]"

{- -- TODO: this mess needs to go! 
compileInstr :: Mapping Pred [Int] -> Instr Int Int -> String
compileInstr idx (LUN n sig) = labelFor n
    ++ ":\n\tdebug(\"In LUN trav " ++ show n ++ "\\n\");"
    ++ "\n\tcheckGraph();\n"
    ++ case definiteLookup sig idx of
        []  -> error "Can't find a node in an empty search space!"
        [s] -> makeCFunctionCall "makeSimpleTrav"
                        [show n, "index(" ++ show s ++ ")" ]
        ss  -> makeCFunctionCall "makeTrav" 
                        (show n:show (length ss):map (\s->"{0, index("++show s++")}") ss)
compileInstr _ (LUE n src tgt Out)
    | src == tgt = "\tdebug(\"In loop trav " ++ show n ++ "\\n\");\n"
                ++ makeCFunctionCallIntArgs "makeLoopTrav" [src, n]
    | otherwise  = "\tdebug(\"In edge trav " ++ show n ++ "\\n\");\n"
                ++ makeCFunctionCallIntArgs "makeEdgeTrav" [src, n, tgt]
compileInstr _ (LBE n a b) = "\tdebug(\"In bidi trav " ++ show n ++ "\\n\");\n"
                          ++ makeCFunctionCallIntArgs "makeBidiEdgeTrav" [a, n, b]
compileInstr _ (XOE src e tgt) = labelFor e
    ++ ":\n\tdebug(\"In XOE trav " ++ show e ++ "\\n\");\n"
    ++ makeCFunctionCallIntArgs "makeExtendOutTrav" [src, e, tgt, 1]
compileInstr _ (XIE src e tgt) = labelFor e
    ++ ":\n\tdebug(\"In XIE trav " ++ show e ++ "\\n\");\n"
    ++ makeCFunctionCallIntArgs "makeExtendInTrav" [src, e, tgt, 1]
compileInstr _ (NEC src tgt)   = makeCFunctionCallIntArgs "makeAntiEdgeTrav" [src, tgt]
-}

makeCFunction :: String -> [String] -> String
makeCFunction name lines = concat [startCFunction name [],  body, "\n}\n"]
    where
        body = intercalate "\n\t" lines

startCFunction :: String -> [(String, String)] -> String
startCFunction name args = concat [ "void " , name, "(" , argString , ") {\n"
                                  , "\tdebug(\"Entering ", name, "()\\n\");" ]
    where
        argString = intercalate ", " [ t ++ " " ++ v | (t,v) <- args ]

endCFunction :: String
endCFunction = "}\n"

asLongAsPossible :: String -> [Int] -> String
asLongAsPossible fname args = concat [ "\tdo {\n\toilrReport();\n\t", makeCFunctionCallIntArgs fname args, "\t} while (boolFlag);\n\tboolFlag=1;\n" ]

makeCFunctionCallIntArgs :: String -> [Int] -> String
makeCFunctionCallIntArgs fname args = makeCFunctionCall fname $ map show args

makeCFunctionCall :: String -> [String] -> String
makeCFunctionCall fname args = '\t':concat [ fname , "(", argStr, ");" ]
    where
        argStr = intercalate ", " args

labelFor :: Int -> String
labelFor n = "trav_no_" ++ show n
 
-}
